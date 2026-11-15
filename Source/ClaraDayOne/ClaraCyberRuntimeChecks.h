#pragma once
#include "ClaraCyberRuntime.h"

// Shared by the standalone executable and UE Automation: no engine, disk or network.
inline std::vector<std::string> ClaraCyberRuntimeChecks() {
    using namespace ClaraCyber;
    std::vector<std::string> failures;
    auto require = [&](bool value, const char* message) { if (!value) failures.emplace_back(message); };
    Model m; m.day=2; m.learn(1); m.choose('A'); m.expect(1,false);
    Terminal terminal;
    const std::string pristine=m.save();
    terminal.command(m,"spiega ping"); terminal.command(m,"ls"); terminal.command(m,"pwd");
    require(m.save()==pristine,"Read-only commands must not mutate model");
    terminal.command(m,"ping -I 192.168.10.30 192.168.20.10");
    require(terminal.busy()&&m.save()==pristine,"Prediction must not award points or observations");
    terminal.tick(m,0.99);
    require(terminal.transcript().find("icmp_seq=")==std::string::npos,"No early reply before one second");
    terminal.tick(m,0.01);
    require(terminal.transcript().find("icmp_seq=1")!=std::string::npos&&m.save()==pristine,"First reply is visible without committing");
    terminal.command(m,"nmap 192.168.10.20");
    require(m.save()==pristine,"Busy terminal rejects another command");
    require(terminal.cancel()&&!terminal.busy(),"Ctrl+C cancels an active ping");
    terminal.tick(m,100);
    require(m.save()==pristine,"Cancelled ping must never commit later");
    terminal.command(m,"ping -I 192.168.10.30 192.168.20.10");terminal.tick(m,3.99);
    require(m.save()==pristine,"Three replies must not complete a test");
    terminal.tick(m,0.01);
    require(!terminal.busy()&&m.observed[1]==1&&(m.rewards[1]&2),"Only fourth reply commits observation and award");
    auto once=m.save();terminal.tick(m,100);require(m.save()==once,"Completion commits exactly once");

    Model blue;blue.day=2;blue.learn(1);blue.choose('B');blue.recognizeLog();blue.reorder({2,3});
    Terminal denied;const std::string before=blue.save();
    denied.command(blue,"ping -I 192.168.10.30 192.168.20.10");denied.tick(blue,3.99);
    require(denied.transcript().find("tempo scaduto")==std::string::npos,"Timeout waits four seconds");
    denied.tick(blue,0.01);
    require(denied.transcript().find("tempo scaduto")!=std::string::npos&&blue.save()==before,"First timeout does not complete blocked verification");
    denied.tick(blue,11.99);require(blue.save()==before,"Blocked verification waits all four probes");
    denied.tick(blue,0.01);
    require(blue.observed[1]==0&&blue.verified[1]&&!denied.busy(),"Expected ACL block is recorded after sixteen seconds");

    Terminal changed;changed.command(blue,"ping -I 192.168.10.50 192.168.20.10");blue.reorder({3,2});
    const std::string afterChange=blue.save();changed.tick(blue,4);
    require(blue.save()==afterChange,"Changed network invalidates pending result without stale commit");
    require(changed.transcript().find("configurazione è cambiata")!=std::string::npos,"Invalidated result explains why it was discarded");
    Terminal invalid;invalid.command(blue,"ping -I 8.8.8.8 192.168.20.10");
    require(!invalid.busy()&&blue.save()==afterChange,"Unauthorized source never schedules a probe");
    invalid.command(blue,"ping -I 192.168.10.30");require(!invalid.busy(),"Malformed ping never schedules a probe");

    Model third;third.day=3;third.learn(1);third.learn(2);third.choose('A');Terminal readOnly;
    const std::string thirdBefore=third.save();
    for(const char* cmd:{"spiega curl","spiega openssl","curl -I http://192.168.20.10/pannello","openssl s_client -connect 192.168.20.10:443"})readOnly.command(third,cmd);
    CurrentTask(third);CommandFeedback(third,"ls","guida");ActionFeedback(third,"report",0,"",false);
    require(third.save()==thirdBefore&&!third.plainSeen&&!third.tlsSeen,"Read-only explanations and headers cannot bypass login or packet inspection");
    const auto rewards=third.rewards;const int red=third.red,bluePoints=third.blue;
    third.hint();third.hint();third.hint();
    require(third.rewards==rewards&&third.red==red&&third.blue==bluePoints&&!third.report(),"Hints do not award points or unlock report");
    return failures;
}

inline std::vector<std::string> ClaraCyberAllPathChecks() {
    using namespace ClaraCyber;
    std::vector<std::string> failures;
    for(char first:{'A','B'})for(char second:{'A','B'})for(char third:{'A','B'}) {
        Model m;
        const std::string path={first,second,third};
        auto require=[&](bool ok,const char* why){if(!ok)failures.push_back(path+": "+why);};
        for(int port:{21,22,23})m.inspectPort(port);require(m.choose(first),"day one choice");
        if(first=='A') {
            m.command("nmap 192.168.10.20");
            for(auto item:std::vector<std::pair<int,int>>{{21,2},{22,3},{23,2},{443,1},{8080,2}})m.classify(item.first,item.second);
            require(m.check22(),"day one verification");
        } else {
            m.recognizeLog();m.inspectPort(8080);for(int port:{21,23,8080})m.setPort(port,false);
            m.command("nmap 192.168.10.20");require(m.openMaterials(),"materials remain available");
        }
        require(m.report(),"day one report");m.opposite();require(m.next(),"day one transition");
        m.learn(1);require(m.choose(second),"day two choice");
        if(second=='A')for(int i=0;i<3;++i)m.expect(i,i==0);
        else {m.recognizeLog();require(m.reorder({2,3}),"day two ACL correction");}
        Terminal terminal;
        for(const char* ip:{"192.168.10.50","192.168.10.30","192.168.10.99"}) {
            terminal.command(m,std::string("ping -I ")+ip+" 192.168.20.10");
            require(terminal.busy(),"day two scheduled ping");terminal.tick(m,16);
            require(!terminal.busy(),"day two completed ping");
        }
        if(second=='A')require(m.identifyRule(1),"day two rule identification");
        require(m.report(),"day two report");m.opposite();require(m.next(),"day two transition");
        m.learn(1);m.learn(2);require(m.choose(third),"day three choice");
        if(third=='A') {
            require(m.login("admin","admin"),"day three login");
            m.networkVision(false);m.networkVision(true);require(m.recognizeWeaknesses(true,true),"day three weaknesses");
        } else {
            m.recognizeLog();require(m.changePassword("VioletRiver!42"),"day three password");
            m.setTls(true);m.verifyOld();m.verifyTeam();require(m.networkVision(true),"day three encrypted inspection");
        }
        require(m.report(),"day three report");m.opposite();require(m.next(),"day three transition");
        Model restored;require(restored.load(m.save())&&restored.save()==m.save(),"completed checkpoint round trip");
        require(m.day==4&&m.red<=100&&m.blue<=100,"all chapters complete with bounded scores");
    }
    return failures;
}
