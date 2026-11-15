#pragma once
#include <array>
#include <set>
#include <string>
#include <utility>
#include <vector>
namespace ClaraCyber{
struct Rule{int id;bool allow;std::string from,to;};
struct Decision{bool allowed;int rule;};
struct Flow{Decision request;bool reply;};
struct PasswordCheck{bool length,uncommon,changed,noUsername;bool valid()const{return length&&uncommon&&changed&&noUsername;}};
class Model{
public:
int day=1,red=0,blue=0;std::array<char,3> path{{0,0,0}};std::array<unsigned,3> rewards{{0,0,0}};std::array<bool,3> perspective{{false,false,false}},finished{{false,false,false}};std::set<std::string> notes;std::array<bool,5> ports{{true,true,true,true,true}};std::vector<Rule> rules;std::array<int,5> classification{{0,0,0,0,0}};std::set<int> inspected;std::array<int,3> expected{{-1,-1,-1}},observed{{-1,-1,-1}};std::array<bool,3> verified{{false,false,false}};bool scanned=false,materials=false,seen22=false,logRecognized=false,plainSeen=false,tlsSeen=false,passwordChanged=false,tls=false,adminEnabled=true,oldRejected=false,teamAccepted=false,trafficChecked=false,weaknesses=false;int hints=0,hintStage=-1,dialogueScene=-1,dialogueOffset=0;std::array<unsigned long long,3> sceneSeen{{0,0,0}};std::array<std::string,3> expectations{{"","",""}};std::vector<std::string> log;
Model();static std::array<std::pair<std::string,std::string>,5> credentials();bool choose(char);bool inspectPort(int);bool learn(int);bool recognizeLog();bool classify(int,int);bool check22();bool setPort(int,bool);bool openMaterials();bool expect(int,bool);Decision evaluate(const std::string&,const std::string&)const;bool reorder(const std::vector<int>&);std::string command(const std::string&);bool identifyRule(int);bool login(const std::string&,const std::string&);bool networkVision(bool);bool changePassword(const std::string&);bool setTls(bool);bool setAdmin(bool);bool verifyOld();bool verifyTeam();bool recognizeWeaknesses(bool,bool);bool report();bool next();bool opposite();std::string hint();std::string save()const;bool load(const std::string&);
static PasswordCheck passwordCheck(const std::string&);Flow flow(const std::string&,const std::string&)const;
private:void award(int);void refresh();};
}
