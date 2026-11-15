#include "ClaraRoutingLab.h"
#include "Dom/JsonObject.h"

namespace
{
bool IPv4(const FString& Text, uint32& Out)
{
    TArray<FString> Parts; Text.ParseIntoArray(Parts,TEXT("."),false);
    if(Parts.Num()!=4)return false;
    Out=0;
    for(const FString& P:Parts)
    {
        if(P.IsEmpty()||P.Len()>3||(P.Len()>1&&P[0]=='0'))return false;
        for(TCHAR C:P)if(C<'0'||C>'9')return false;
        const int32 N=FCString::Atoi(*P); if(N>255)return false;
        Out=(Out<<8)|uint32(N);
    }
    return true;
}
bool Mask(const FString& Text,uint32& Out,int32& Prefix)
{
    if(!IPv4(Text,Out))return false;
    const uint32 Inverse=~Out;
    if((Inverse&(Inverse+1))!=0)return false;
    Prefix=0; for(uint32 Bits=Out;Bits;Bits<<=1)++Prefix;
    return true;
}
bool Host(const FString& Text,const FString& MaskText)
{
    uint32 A=0,M=0;int32 P=0;
    if(!IPv4(Text,A)||!Mask(MaskText,M,P)||P<1||P>30)return false;
    return (A>>24)>0&&(A>>24)<224&&(A>>24)!=127&&(A&~M)!=0&&(A&~M)!=~M;
}
bool SameNetwork(const FString& A,const FString& B,const FString& MaskText)
{
    uint32 X=0,Y=0,M=0;int32 P=0;
    return IPv4(A,X)&&IPv4(B,Y)&&Mask(MaskText,M,P)&&(X&M)==(Y&M);
}
FString Address(uint32 A)
{return FString::Printf(TEXT("%u.%u.%u.%u"),(A>>24)&255,(A>>16)&255,(A>>8)&255,A&255);}
bool ValidName(const FString& Name)
{
    if(Name.IsEmpty()||Name.Len()>63||!FChar::IsAlpha(Name[0]))return false;
    for(TCHAR C:Name)if(!FChar::IsAlnum(C)&&C!='-')return false;
    return true;
}
FString CLIError(const TCHAR* Message){return FString(TEXT("% "))+Message;}
FString SwitchMask(const FClaraSwitchCLI& Switch)
{
    const auto Config=Switch.ExportState()->GetObjectField(TEXT("running"));
    for(const auto& Value:Config->GetArrayField(TEXT("interfaces")))
    {
        const auto I=Value->AsObject();
        if(I->GetStringField(TEXT("address"))==Switch.GetManagementAddress())return I->GetStringField(TEXT("mask"));
    }
    return FString();
}
}

FClaraRoutingLab::FClaraRoutingLab(){Reset();}
FString FClaraRoutingLab::Edge(const FString& A,const FString& B){return A<B?A+TEXT("|")+B:B+TEXT("|")+A;}
const FClaraRoutingLab::FRouter* FClaraRoutingLab::Router(const FString& Id) const{return Id==TEXT("R1")?&R1:Id==TEXT("R2")?&R2:nullptr;}
FClaraRoutingLab::FRouter* FClaraRoutingLab::Router(const FString& Id){return Id==TEXT("R1")?&R1:Id==TEXT("R2")?&R2:nullptr;}
void FClaraRoutingLab::Reset()
{
    R1=FRouter(); R2=FRouter(); SW2.Reset(); Cables.Empty();
    R1.Running.Hostname=TEXT("R1");
    R1.Running.Interfaces[0]={TEXT("192.168.1.1"),TEXT("255.255.255.0"),true};
    R1.Startup=R1.Running; R2.Startup=R2.Running;
    SetCable(TEXT("PC2"),TEXT("SW1"),true); SetCable(TEXT("SW1"),TEXT("R1"),true);
}
void FClaraRoutingLab::SetCable(const FString& A,const FString& B,bool Connected)
{
    const FString E=Edge(A,B);
    if(E!=Edge(TEXT("PC2"),TEXT("SW1"))&&E!=Edge(TEXT("SW1"),TEXT("R1"))&&
       E!=Edge(TEXT("R1"),TEXT("R2"))&&E!=Edge(TEXT("R2"),TEXT("SW2")))return;
    if(Connected)Cables.Add(E);else Cables.Remove(E);
}
bool FClaraRoutingLab::HasCable(const FString& A,const FString& B) const{return Cables.Contains(Edge(A,B));}
bool FClaraRoutingLab::IsAwaitingInput(const FString& Device) const
{
    if(Device==TEXT("SW2"))return SW2.IsAwaitingInput();
    const FRouter* R=Router(Device);return R&&(R->AwaitingSave||R->AwaitingReload);
}
bool FClaraRoutingLab::IsReadingBanner(const FString& Device) const
{return Device==TEXT("SW2")&&SW2.IsReadingBanner();}
FString FClaraRoutingLab::GetPrompt(const FString& Device) const
{
    if(Device==TEXT("SW2"))return SW2.GetPrompt();
    if(Device==TEXT("PC2"))return TEXT("PC2> ");
    const FRouter* R=Router(Device);if(!R)return Device+TEXT("> ");
    if(R->AwaitingSave)return TEXT("Destination filename [startup-config]? ");
    if(R->AwaitingReload)return TEXT("Proceed with reload? [confirm] ");
    const TCHAR* Tail=R->Mode==EMode::User?TEXT("> "):R->Mode==EMode::Privileged?TEXT("# "):
        R->Mode==EMode::Interface?TEXT("(config-if)# "):R->Mode==EMode::Console?TEXT("(config-line)# "):TEXT("(config)# ");
    return R->Running.Hostname+Tail;
}
FString FClaraRoutingLab::Execute(const FString& Device,const FString& Input)
{
    if(Input.Len()>1024||Input.Contains(TEXT("\n"))||Input.Contains(TEXT("\r")))return CLIError(TEXT("Inserisci un comando per riga, massimo 1024 caratteri."));
    if(Device==TEXT("LAPTOP"))return TEXT("Il portatile serve solo alla console. Ping e traceroute si eseguono da PC2.");
    if(FRouter* R=Router(Device))return ExecuteRouter(*R,Device,Input);
    TArray<FString> Words;Input.TrimStartAndEnd().ParseIntoArrayWS(Words);
    const FString Cmd=Words.Num()?Words[0].ToLower():FString();
    if(Device==TEXT("SW2")&&!SW2.IsAwaitingInput())
    {
        if(Words.Num()==2&&Cmd==TEXT("ping"))return Ping(Device,Words[1]);
        if(Words.Num()==2&&Cmd==TEXT("traceroute"))return Traceroute(Device,Words[1]);
    }
    if(Device==TEXT("SW2"))return SW2.Execute(Input);
    if(Device!=TEXT("PC2"))return CLIError(TEXT("SW1 serve solo ai collegamenti; scegli PC2, R1, R2 o SW2."));
    if(Words.Num()==2&&Cmd==TEXT("ping"))return Ping(Device,Words[1]);
    if(Words.Num()==2&&Cmd==TEXT("traceroute"))return Traceroute(Device,Words[1]);
    if(Cmd==TEXT("ipconfig"))return TEXT("PC2\nIPv4: 192.168.1.11\nMask: 255.255.255.0\nGateway: 192.168.1.1");
    return TEXT("PC2: ipconfig | ping <IPv4> | traceroute <IPv4>.\nModello didattico locale: nessun comando viene eseguito sul computer.");
}

FString FClaraRoutingLab::ExecuteRouter(FRouter& R,const FString& Id,const FString& Input)
{
    const FString Line=Input.TrimStartAndEnd();
    if(R.AwaitingSave)
    {
        R.AwaitingSave=false;
        if(!Line.IsEmpty()&&Line!=TEXT("startup-config"))return CLIError(TEXT("Salvataggio annullato: destinazione non supportata."));
        R.Startup=R.Running;return TEXT("Building configuration...\n[OK]");
    }
    if(R.AwaitingReload)
    {
        R.AwaitingReload=false;
        if(!Line.IsEmpty()&&Line.ToLower()!=TEXT("y"))return TEXT("Riavvio annullato.");
        R.Running=R.Startup;R.Mode=EMode::User;return TEXT("Dispositivo riavviato dalla startup-config.");
    }
    TArray<FString> W;Line.ParseIntoArrayWS(W);if(W.IsEmpty())return FString();
    const FString Lower=Line.ToLower();
    if(Lower==TEXT("?")||Lower==TEXT("help"))return TEXT("enable | configure terminal | hostname <nome> | enable secret <password>\nline console 0 | password <password> | login | exit | end\ninterface gigabitethernet 0/0 (oppure 0/1) | ip address <IP> <mask> | no shutdown\nip route <rete> <mask> <next-hop> | no ip route <rete> <mask> <next-hop>\nshow ip route | show ip interface brief | show running-config | show startup-config\nping <IP> | traceroute <IP> | copy running-config startup-config | reload\nI comandi di configurazione richiedono configure terminal. In configurazione: do show ...");
    if(Lower.StartsWith(TEXT("do "))&&R.Mode!=EMode::User&&R.Mode!=EMode::Privileged)
    {
        const EMode Previous=R.Mode;R.Mode=EMode::Privileged;
        const FString Result=ExecuteRouter(R,Id,Line.Mid(3));R.Mode=Previous;return Result;
    }
    if(Lower==TEXT("end")){if(R.Mode==EMode::User)return CLIError(TEXT("Entra prima con enable."));R.Mode=EMode::Privileged;return FString();}
    if(Lower==TEXT("exit")){R.Mode=(R.Mode==EMode::Interface||R.Mode==EMode::Console)?EMode::Config:R.Mode==EMode::Config?EMode::Privileged:EMode::User;return FString();}
    if(Lower==TEXT("enable")||Lower==TEXT("en")){if(R.Mode==EMode::User)R.Mode=EMode::Privileged;return FString();}
    if(Lower==TEXT("disable")){R.Mode=EMode::User;return FString();}
    if(R.Mode==EMode::User||R.Mode==EMode::Privileged)
    {
        if(Lower==TEXT("show ip route")||Lower==TEXT("sh ip route"))return ShowRoutes(Id);
        if(Lower==TEXT("show ip interface brief")||Lower==TEXT("sh ip int br"))return ShowInterfaces(Id);
        if(W.Num()==2&&W[0].Equals(TEXT("ping"),ESearchCase::IgnoreCase))return Ping(Id,W[1]);
        if(W.Num()==2&&W[0].Equals(TEXT("traceroute"),ESearchCase::IgnoreCase))return Traceroute(Id,W[1]);
    }
    if(R.Mode==EMode::Privileged)
    {
        if(Lower==TEXT("configure terminal")||Lower==TEXT("conf t")){R.Mode=EMode::Config;return TEXT("Enter configuration commands, one per line. End with end.");}
        if(Lower==TEXT("show running-config")||Lower==TEXT("sh run"))return RenderConfig(R.Running);
        if(Lower==TEXT("show startup-config")||Lower==TEXT("sh start"))return RenderConfig(R.Startup);
        if(Lower==TEXT("copy running-config startup-config")||Lower==TEXT("copy run start")){R.AwaitingSave=true;return TEXT("Destination filename [startup-config]?");}
        if(Lower==TEXT("write memory")||Lower==TEXT("wr")){R.Startup=R.Running;return TEXT("[OK]");}
        if(Lower==TEXT("reload")){R.AwaitingReload=true;return TEXT("Le modifiche non salvate andranno perse. Proceed with reload? [confirm]");}
    }
    if(R.Mode==EMode::Config)
    {
        if(W.Num()==2&&W[0].Equals(TEXT("hostname"),ESearchCase::IgnoreCase))
        {if(!ValidName(W[1]))return CLIError(TEXT("Nome non valido."));R.Running.Hostname=W[1];return FString();}
        if(W.Num()==3&&W[0].ToLower()==TEXT("enable")&&W[1].ToLower()==TEXT("secret"))
        {if(W[2].Len()>256)return CLIError(TEXT("Password troppo lunga."));R.Running.Secret=W[2];return FString();}
        if(Lower==TEXT("line console 0")){R.Mode=EMode::Console;return FString();}
        if(W.Num()>=2&&(W[0].ToLower()==TEXT("interface")||W[0].ToLower()==TEXT("int")))
        {
            FString Name=Line.Mid(W[0].Len()).Replace(TEXT(" "),TEXT("")).ToLower();
            int32 Index=Name.EndsWith(TEXT("0/0"))?0:Name.EndsWith(TEXT("0/1"))?1:-1;
            if(Index<0||(Name!=FString::Printf(TEXT("gigabitethernet0/%d"),Index)&&Name!=FString::Printf(TEXT("gi0/%d"),Index)&&Name!=FString::Printf(TEXT("g0/%d"),Index)))
                return CLIError(TEXT("Interfacce disponibili: gigabitethernet 0/0 e 0/1."));
            R.Interface=Index;R.Mode=EMode::Interface;return FString();
        }
        const bool Remove=W.Num()>0&&W[0].ToLower()==TEXT("no");const int32 Offset=Remove?1:0;
        if(W.Num()==Offset+5&&W[Offset].ToLower()==TEXT("ip")&&W[Offset+1].ToLower()==TEXT("route"))
        {
            const FString Net=W[Offset+2],MaskText=W[Offset+3],Next=W[Offset+4];
            uint32 N=0,M=0,H=0;int32 Prefix=0;
            if(!IPv4(Net,N)||!Mask(MaskText,M,Prefix)||(N&M)!=N||!IPv4(Next,H))return CLIError(TEXT("Rete, mask o next-hop non validi. La destinazione deve essere un indirizzo di rete."));
            if(Remove)
            {const int32 Count=R.Running.Routes.RemoveAll([&](const FRoute& Route){return Route.Network==Net&&Route.Mask==MaskText&&Route.NextHop==Next;});return Count?FString():CLIError(TEXT("Rotta non presente."));}
            bool Reachable=false;
            for(const auto& I:R.Running.Interfaces)if(Host(Next,I.Mask)&&Next!=I.Address&&SameNetwork(Next,I.Address,I.Mask))Reachable=true;
            if(!Reachable)return CLIError(TEXT("Il next-hop deve essere un host di una rete collegata, diverso dall'IP del router. Rete e broadcast non sono host."));
            R.Running.Routes.RemoveAll([&](const FRoute& Route){return Route.Network==Net&&Route.Mask==MaskText;});
            if(R.Running.Routes.Num()>=64)return CLIError(TEXT("Limite del laboratorio: 64 rotte statiche."));
            R.Running.Routes.Add({Net,MaskText,Next});return FString();
        }
    }
    if(R.Mode==EMode::Console)
    {
        if(W.Num()==2&&W[0].ToLower()==TEXT("password")){if(W[1].Len()>256)return CLIError(TEXT("Password troppo lunga."));R.Running.ConsolePassword=W[1];return FString();}
        if(Lower==TEXT("login")){R.Running.ConsoleLogin=true;return FString();}
    }
    if(R.Mode==EMode::Interface)
    {
        auto& I=R.Running.Interfaces[R.Interface];
        if(W.Num()==4&&W[0].ToLower()==TEXT("ip")&&W[1].ToLower()==TEXT("address"))
        {
            if(!Host(W[2],W[3]))return CLIError(TEXT("Indirizzo host o mask non validi: usa una mask contigua e non assegnare rete o broadcast."));
            const auto& Other=R.Running.Interfaces[1-R.Interface];
            if(!Other.Address.IsEmpty()&&(SameNetwork(W[2],Other.Address,W[3])||SameNetwork(W[2],Other.Address,Other.Mask)))return CLIError(TEXT("Le due interfacce devono appartenere a reti distinte."));
            I.Address=W[2];I.Mask=W[3];return FString();
        }
        if(Lower==TEXT("no shutdown")||Lower==TEXT("no shut")){I.Up=true;return FString();}
        if(Lower==TEXT("shutdown")||Lower==TEXT("shut")){I.Up=false;return FString();}
    }
    return CLIError(TEXT("Comando non valido in questo contesto. Usa help; per configurare: enable, configure terminal."));
}

FString FClaraRoutingLab::RenderConfig(const FConfig& C) const
{
    FString Out=TEXT("hostname ")+C.Hostname+TEXT("\n");
    if(!C.Secret.IsEmpty())Out+=TEXT("enable secret <impostata>\n");
    Out+=TEXT("line console 0\n");if(!C.ConsolePassword.IsEmpty())Out+=TEXT(" password <impostata>\n");if(C.ConsoleLogin)Out+=TEXT(" login\n");
    for(int32 J=0;J<2;++J)
    {
        const auto& I=C.Interfaces[J];Out+=FString::Printf(TEXT("interface GigabitEthernet0/%d\n"),J);
        if(!I.Address.IsEmpty())Out+=TEXT(" ip address ")+I.Address+TEXT(" ")+I.Mask+TEXT("\n");
        Out+=I.Up?TEXT(" no shutdown\n"):TEXT(" shutdown\n");
    }
    for(const auto& Route:C.Routes)Out+=TEXT("ip route ")+Route.Network+TEXT(" ")+Route.Mask+TEXT(" ")+Route.NextHop+TEXT("\n");
    return Out;
}
bool FClaraRoutingLab::InterfaceActive(const FString& Id,int32 Index) const
{
    const FRouter* R=Router(Id);if(!R||Index<0||Index>1)return false;
    const auto& I=R->Running.Interfaces[Index];if(!I.Up||I.Address.IsEmpty())return false;
    if(Id==TEXT("R1")&&Index==0)return HasCable(TEXT("R1"),TEXT("SW1"));
    if(Id==TEXT("R2")&&Index==1)return HasCable(TEXT("R2"),TEXT("SW2"));
    const auto& Peer=(Id==TEXT("R1")?R2:R1).Running.Interfaces[Id==TEXT("R1")?0:1];
    return HasCable(TEXT("R1"),TEXT("R2"))&&Peer.Up&&!Peer.Address.IsEmpty();
}
FString FClaraRoutingLab::ShowInterfaces(const FString& Id) const
{
    FString Out=TEXT("Interface              IP-Address       Status                 Protocol\n");
    const FRouter* R=Router(Id);if(!R)return Out;
    for(int32 I=0;I<2;++I)
    {
        const auto& Port=R->Running.Interfaces[I];const bool Active=InterfaceActive(Id,I);
        Out+=FString::Printf(TEXT("GigabitEthernet0/%d     %-16s %-22s %s\n"),I,Port.Address.IsEmpty()?TEXT("unassigned"):*Port.Address,!Port.Up?TEXT("administratively down"):Active?TEXT("up"):TEXT("down"),Active?TEXT("up"):TEXT("down"));
    }
    return Out;
}
FString FClaraRoutingLab::ShowRoutes(const FString& Id) const
{
    FString Out=TEXT("C - connected, L - local, S - static, S* - default\n");const FRouter* R=Router(Id);if(!R)return Out;
    for(int32 I=0;I<2;++I)if(InterfaceActive(Id,I))
    {
        uint32 A=0,M=0;int32 P=0;const auto& Port=R->Running.Interfaces[I];IPv4(Port.Address,A);Mask(Port.Mask,M,P);
        Out+=FString::Printf(TEXT("C %s/%d is directly connected, GigabitEthernet0/%d\nL %s/32 is directly connected, GigabitEthernet0/%d\n"),*Address(A&M),P,I,*Port.Address,I);
    }
    for(const auto& Route:R->Running.Routes)
    {
        uint32 M=0;int32 P=0;Mask(Route.Mask,M,P);bool Resolved=false;
        for(int32 I=0;I<2;++I)if(InterfaceActive(Id,I)&&SameNetwork(Route.NextHop,R->Running.Interfaces[I].Address,R->Running.Interfaces[I].Mask))Resolved=true;
        if(Resolved)Out+=FString::Printf(TEXT("%s %s/%d via %s\n"),P==0?TEXT("S*"):TEXT("S"),*Route.Network,P,*Route.NextHop);
    }
    return Out;
}
bool FClaraRoutingLab::FindRoute(const FString& Id,const FString& Target,int32& Interface,FString& NextHop) const
{
    const FRouter* R=Router(Id);uint32 Dest=0;if(!R||!IPv4(Target,Dest))return false;
    int32 Best=-1;Interface=-1;NextHop.Empty();
    for(int32 I=0;I<2;++I)if(InterfaceActive(Id,I))
    {
        uint32 A=0,M=0;int32 P=0;const auto& Port=R->Running.Interfaces[I];IPv4(Port.Address,A);Mask(Port.Mask,M,P);
        if((Dest&M)==(A&M)&&P>Best){Best=P;Interface=I;NextHop=Target;}
    }
    for(const auto& Route:R->Running.Routes)
    {
        uint32 Net=0,M=0;int32 P=0;IPv4(Route.Network,Net);Mask(Route.Mask,M,P);
        if((Dest&M)!=Net||P<=Best)continue;
        for(int32 I=0;I<2;++I)if(InterfaceActive(Id,I)&&SameNetwork(Route.NextHop,R->Running.Interfaces[I].Address,R->Running.Interfaces[I].Mask))
        {Best=P;Interface=I;NextHop=Route.NextHop;break;}
    }
    return Interface>=0;
}
FString FClaraRoutingLab::AddressOwner(const FString& IP) const
{
    if(IP==TEXT("192.168.1.11"))return TEXT("PC2");
    if(IP==SW2.GetManagementAddress()&&SW2.IsSVIUp())return TEXT("SW2");
    for(const FString& Id:{FString(TEXT("R1")),FString(TEXT("R2"))})
        for(int32 I=0;I<2;++I)if(InterfaceActive(Id,I)&&Router(Id)->Running.Interfaces[I].Address==IP)return Id;
    return FString();
}
FString FClaraRoutingLab::SourceAddress(const FString& Device,const FString& Target) const
{
    if(Device==TEXT("PC2"))return TEXT("192.168.1.11");
    if(Device==TEXT("SW2"))return SW2.IsSVIUp()?SW2.GetManagementAddress():FString();
    const FRouter* R=Router(Device);if(!R)return FString();
    for(int32 I=0;I<2;++I)if(InterfaceActive(Device,I)&&R->Running.Interfaces[I].Address==Target)return Target;
    int32 I=-1;FString Next;if(FindRoute(Device,Target,I,Next))return R->Running.Interfaces[I].Address;
    for(int32 J=0;J<2;++J)if(InterfaceActive(Device,J))return R->Running.Interfaces[J].Address;
    return FString();
}
bool FClaraRoutingLab::Path(const FString& Source,const FString& Target,TArray<FString>& Hops) const
{
    Hops.Empty();const FString Origin=AddressOwner(Source);if(Origin.IsEmpty())return false;
    if(Source==Target)return true;
    FString Current=Origin;
    if(Origin==TEXT("PC2"))
    {
        const auto& I=R1.Running.Interfaces[0];
        if(!HasCable(TEXT("PC2"),TEXT("SW1"))||!InterfaceActive(TEXT("R1"),0)||I.Address!=TEXT("192.168.1.1")||!SameNetwork(Source,I.Address,I.Mask))return false;
        Current=TEXT("R1");Hops.Add(I.Address);
    }
    else if(Origin==TEXT("SW2"))
    {
        const auto& I=R2.Running.Interfaces[1];const FString Next=SameNetwork(Target,Source,SwitchMask(SW2))?Target:SW2.GetGateway();
        if(!InterfaceActive(TEXT("R2"),1)||!SameNetwork(Source,I.Address,I.Mask)||Next!=I.Address||!SameNetwork(Source,Next,SwitchMask(SW2)))return false;
        Current=TEXT("R2");Hops.Add(I.Address);
    }
    TSet<FString> Visited;
    for(int32 Step=0;Step<8;++Step)
    {
        if(AddressOwner(Target)==Current)
        {if(Hops.IsEmpty()||Hops.Last()!=Target)Hops.Add(Target);return true;}
        if(Visited.Contains(Current))return false;Visited.Add(Current);
        int32 Port=-1;FString Next;if(!FindRoute(Current,Target,Port,Next))return false;
        if(Current==TEXT("R1")&&Port==0)
        {
            if(Target==TEXT("192.168.1.11")&&Next==Target&&HasCable(TEXT("PC2"),TEXT("SW1")))
            {Hops.Add(Target);return true;}return false;
        }
        if(Current==TEXT("R2")&&Port==1)
        {
            if(SW2.IsSVIUp()&&Target==SW2.GetManagementAddress()&&Next==Target&&SameNetwork(Target,R2.Running.Interfaces[1].Address,SwitchMask(SW2)))
            {Hops.Add(Target);return true;}return false;
        }
        const FString Peer=Current==TEXT("R1")?TEXT("R2"):TEXT("R1");const int32 PeerPort=Current==TEXT("R1")?0:1;
        const auto& PeerInterface=Router(Peer)->Running.Interfaces[PeerPort];
        if(!InterfaceActive(Peer,PeerPort)||PeerInterface.Address!=Next||!SameNetwork(Next,Router(Current)->Running.Interfaces[Port].Address,Router(Current)->Running.Interfaces[Port].Mask))return false;
        // Collapse a routing loop into the first missing hop; this lesson has at most two routers.
        if(Visited.Contains(Peer))return false;
        Current=Peer;Hops.Add(PeerInterface.Address);
    }
    return false;
}
FString FClaraRoutingLab::Ping(const FString& Source,const FString& Target) const
{
    uint32 Parsed=0;if(!IPv4(Target,Parsed))return CLIError(TEXT("Indirizzo IPv4 non valido."));
    const FString Origin=SourceAddress(Source,Target);if(Origin.IsEmpty())return CLIError(TEXT("Sorgente non disponibile. Il portatile fa solo la console."));
    TArray<FString> Forward,Return;
    if(!Path(Origin,Target,Forward))return TEXT("Destination host unreachable.\nControlla cavi, interfacce e rotta verso la destinazione.");
    if(!Path(Target,Origin,Return))return TEXT("Request timed out.\nL'andata arriva, ma la risposta non torna: controlla gateway e rotta di ritorno.");
    return TEXT("Reply from ")+Target+TEXT(": bytes=32 time<1ms TTL=64\n4 inviati, 4 ricevuti, 0 persi.");
}
FString FClaraRoutingLab::Traceroute(const FString& Source,const FString& Target) const
{
    uint32 Parsed=0;if(!IPv4(Target,Parsed))return CLIError(TEXT("Indirizzo IPv4 non valido."));
    const FString Origin=SourceAddress(Source,Target);if(Origin.IsEmpty())return CLIError(TEXT("Sorgente non disponibile. Usa PC2 per le verifiche del copione."));
    TArray<FString> Forward;const bool Reached=Path(Origin,Target,Forward);
    FString Out=TEXT("Traceroute to ")+Target+TEXT(" (modello locale)\n");int32 Hop=1;
    for(const FString& AddressValue:Forward)
    {
        TArray<FString> Return;if(!Path(AddressValue,Origin,Return)){Out+=FString::Printf(TEXT("%d  * * *\n"),Hop);return Out;}
        Out+=FString::Printf(TEXT("%d  <1 ms  %s\n"),Hop++,*AddressValue);
    }
    if(!Reached)Out+=FString::Printf(TEXT("%d  * * *\n"),Hop);
    return Out;
}

bool FClaraRoutingLab::IsStartupSaved(const FString& Device) const
{
    if(Device==TEXT("SW2"))return SW2.IsStartupSaved();
    const FRouter* R=Router(Device);return R&&!R->AwaitingSave&&RenderConfig(R->Running)==RenderConfig(R->Startup)&&
        R->Running.Secret==R->Startup.Secret&&R->Running.ConsolePassword==R->Startup.ConsolePassword;
}
bool FClaraRoutingLab::HasRoute(const FString& Device,const FString& Network,const FString& MaskText,const FString& NextHop) const
{
    const FRouter* R=Router(Device);if(!R)return false;
    for(const auto& Route:R->Running.Routes)if(Route.Network==Network&&Route.Mask==MaskText&&Route.NextHop==NextHop)return true;
    return false;
}
bool FClaraRoutingLab::IsDeviceConfigured(const FString& Device,bool IncludeSaved) const
{
    if(IncludeSaved&&!IsStartupSaved(Device))return false;
    if(Device==TEXT("SW2"))
    {
        const auto C=SW2.ExportState()->GetObjectField(TEXT("running"));
        return SW2.GetHostname()==TEXT("SW2")&&SW2.GetManagementAddress()==TEXT("192.168.2.2")&&
            SwitchMask(SW2)==TEXT("255.255.255.0")&&SW2.IsSVIUp()&&SW2.GetGateway()==TEXT("192.168.2.1")&&
            C->GetStringField(TEXT("enableSecret"))==TEXT("Cisco123")&&C->GetStringField(TEXT("consolePassword"))==TEXT("Console123")&&
            C->GetBoolField(TEXT("consoleLogin"))&&!C->GetBoolField(TEXT("consoleLocal"));
    }
    const FRouter* R=Router(Device);if(!R||R->Running.Hostname!=Device)return false;
    const auto& A=R->Running.Interfaces[0];const auto& B=R->Running.Interfaces[1];
    if(!A.Up||!B.Up)return false;
    if(Device==TEXT("R1"))return A.Address==TEXT("192.168.1.1")&&A.Mask==TEXT("255.255.255.0")&&B.Address==TEXT("10.0.0.1")&&B.Mask==TEXT("255.255.255.252");
    return A.Address==TEXT("10.0.0.2")&&A.Mask==TEXT("255.255.255.252")&&B.Address==TEXT("192.168.2.1")&&B.Mask==TEXT("255.255.255.0")&&
        R->Running.Secret==TEXT("Cisco123")&&R->Running.ConsolePassword==TEXT("Console123")&&R->Running.ConsoleLogin;
}
bool FClaraRoutingLab::IsBaseConfigured(bool IncludeSaved) const
{return IsDeviceConfigured(TEXT("R1"),IncludeSaved)&&IsDeviceConfigured(TEXT("R2"),IncludeSaved)&&IsDeviceConfigured(TEXT("SW2"),IncludeSaved);}
TArray<FString> FClaraRoutingLab::Validate(bool IncludeSaved) const
{
    TArray<FString> Problems;
    for(const TPair<FString,FString>& Pair:TArray<TPair<FString,FString>>{{TEXT("PC2"),TEXT("SW1")},{TEXT("SW1"),TEXT("R1")},{TEXT("R1"),TEXT("R2")},{TEXT("R2"),TEXT("SW2")}})
        if(!HasCable(Pair.Key,Pair.Value))Problems.Add(TEXT("Manca il collegamento ")+Pair.Key+TEXT(" — ")+Pair.Value+TEXT("."));
    for(const FString& Id:{FString(TEXT("R1")),FString(TEXT("R2")),FString(TEXT("SW2"))})
    {
        if(!IsDeviceConfigured(Id,false))Problems.Add(Id+TEXT(": controlla nome, accessi, IP, mask e interfacce; su SW2 anche il gateway."));
        if(IncludeSaved&&!IsStartupSaved(Id))Problems.Add(TEXT("La configurazione di ")+Id+TEXT(" non è salvata."));
    }
    if(!HasRoute(TEXT("R1"),TEXT("192.168.2.0"),TEXT("255.255.255.0"),TEXT("10.0.0.2")))Problems.Add(TEXT("La rotta di R1 verso la rete B non è corretta."));
    if(!HasRoute(TEXT("R2"),TEXT("0.0.0.0"),TEXT("0.0.0.0"),TEXT("10.0.0.1")))Problems.Add(TEXT("La rotta di ritorno di R2 non è corretta."));
    for(const FString& Target:{FString(TEXT("192.168.1.1")),FString(TEXT("10.0.0.2")),FString(TEXT("192.168.2.2"))})
        if(!Ping(TEXT("PC2"),Target).StartsWith(TEXT("Reply from ")))Problems.Add(Target+TEXT(" non risponde al ping da PC2."));
    const FString Trace=Traceroute(TEXT("PC2"),TEXT("192.168.2.2"));
    if(Trace.Contains(TEXT("* * *"))||!Trace.Contains(TEXT("1  <1 ms  192.168.1.1"))||!Trace.Contains(TEXT("2  <1 ms  10.0.0.2"))||!Trace.Contains(TEXT("3  <1 ms  192.168.2.2")))Problems.Add(TEXT("Il traceroute da PC2 a SW2 non è completo."));
    return Problems;
}
void FClaraRoutingLab::InjectFaults()
{
    // Deliberate faulty saved state; ordinary CLI input cannot assign the /30 broadcast as next hop.
    for(auto& Route:R1.Running.Routes)if(Route.Network==TEXT("192.168.2.0")&&Route.Mask==TEXT("255.255.255.0"))Route.NextHop=TEXT("10.0.0.3");
    R2.Running.Routes.RemoveAll([](const FRoute& Route){return Route.Network==TEXT("0.0.0.0")&&Route.Mask==TEXT("0.0.0.0");});
    R2.Running.Interfaces[1].Up=false;
    R1.Startup=R1.Running;R2.Startup=R2.Running;
    auto State=SW2.ExportState();
    State->GetObjectField(TEXT("running"))->SetStringField(TEXT("gateway"),TEXT("192.168.1.1"));
    State->GetObjectField(TEXT("startup"))->SetStringField(TEXT("gateway"),TEXT("192.168.1.1"));
    SW2.ImportState(State);
    R1.AwaitingSave=R2.AwaitingSave=false;R1.AwaitingReload=R2.AwaitingReload=false;
}
void FClaraRoutingLab::Reboot()
{
    for(FRouter* R:{&R1,&R2}){R->Running=R->Startup;R->Mode=EMode::User;R->AwaitingSave=R->AwaitingReload=false;R->Interface=0;}
    SW2.ReloadFromStartup();
}
TSharedRef<FJsonObject> FClaraRoutingLab::WriteConfig(const FConfig& C)
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("hostname"),C.Hostname);O->SetStringField(TEXT("secret"),C.Secret);
    O->SetStringField(TEXT("consolePassword"),C.ConsolePassword);O->SetBoolField(TEXT("consoleLogin"),C.ConsoleLogin);
    TArray<TSharedPtr<FJsonValue>> Interfaces,Routes;
    for(const auto& I:C.Interfaces)
    {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("address"),I.Address);J->SetStringField(TEXT("mask"),I.Mask);J->SetBoolField(TEXT("up"),I.Up);Interfaces.Add(MakeShared<FJsonValueObject>(J));}
    for(const auto& R:C.Routes)
    {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("network"),R.Network);J->SetStringField(TEXT("mask"),R.Mask);J->SetStringField(TEXT("nextHop"),R.NextHop);Routes.Add(MakeShared<FJsonValueObject>(J));}
    O->SetArrayField(TEXT("interfaces"),Interfaces);O->SetArrayField(TEXT("routes"),Routes);return O;
}
bool FClaraRoutingLab::ReadConfig(const TSharedPtr<FJsonObject>& O,FConfig& C)
{
    if(!O.IsValid()||!O->TryGetStringField(TEXT("hostname"),C.Hostname)||!ValidName(C.Hostname)||
       !O->TryGetStringField(TEXT("secret"),C.Secret)||C.Secret.Len()>256||!O->TryGetStringField(TEXT("consolePassword"),C.ConsolePassword)||C.ConsolePassword.Len()>256||
       !O->TryGetBoolField(TEXT("consoleLogin"),C.ConsoleLogin))return false;
    const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
    if(!O->TryGetArrayField(TEXT("interfaces"),Values)||Values->Num()!=2)return false;
    for(int32 Index=0;Index<2;++Index)
    {
        const TSharedPtr<FJsonObject>* J=nullptr;auto& I=C.Interfaces[Index];
        if(!(*Values)[Index].IsValid()||!(*Values)[Index]->TryGetObject(J)||!J->IsValid()||
           !(*J)->TryGetStringField(TEXT("address"),I.Address)||!(*J)->TryGetStringField(TEXT("mask"),I.Mask)||!(*J)->TryGetBoolField(TEXT("up"),I.Up))return false;
        if(I.Address.IsEmpty()? !I.Mask.IsEmpty() : !Host(I.Address,I.Mask))return false;
    }
    if(!O->TryGetArrayField(TEXT("routes"),Values)||Values->Num()>64)return false;
    C.Routes.Empty();
    for(const auto& Value:*Values)
    {
        const TSharedPtr<FJsonObject>* J=nullptr;FRoute R;uint32 N=0,M=0,H=0;int32 Prefix=0;
        if(!Value.IsValid()||!Value->TryGetObject(J)||!J->IsValid()||!(*J)->TryGetStringField(TEXT("network"),R.Network)||!(*J)->TryGetStringField(TEXT("mask"),R.Mask)||!(*J)->TryGetStringField(TEXT("nextHop"),R.NextHop)||
           !IPv4(R.Network,N)||!Mask(R.Mask,M,Prefix)||(N&M)!=N||!IPv4(R.NextHop,H))return false;
        C.Routes.Add(R);
    }
    return true;
}
TSharedRef<FJsonObject> FClaraRoutingLab::ExportState() const
{
    auto State=MakeShared<FJsonObject>();State->SetNumberField(TEXT("version"),1);
    for(const FString& Id:{FString(TEXT("R1")),FString(TEXT("R2"))})
    {
        const FRouter* R=Router(Id);auto O=MakeShared<FJsonObject>();
        O->SetObjectField(TEXT("running"),WriteConfig(R->Running));O->SetObjectField(TEXT("startup"),WriteConfig(R->Startup));
        O->SetNumberField(TEXT("mode"),static_cast<int32>(R->Mode));O->SetNumberField(TEXT("interface"),R->Interface);
        O->SetBoolField(TEXT("awaitingSave"),R->AwaitingSave);O->SetBoolField(TEXT("awaitingReload"),R->AwaitingReload);State->SetObjectField(Id,O);
    }
    State->SetObjectField(TEXT("SW2"),SW2.ExportState());TArray<TSharedPtr<FJsonValue>> Links;
    TArray<FString> Sorted=Cables.Array();Sorted.Sort();for(const FString& Link:Sorted)Links.Add(MakeShared<FJsonValueString>(Link));
    State->SetArrayField(TEXT("cables"),Links);return State;
}
bool FClaraRoutingLab::ImportState(const TSharedPtr<FJsonObject>& State)
{
    int32 Version=0;if(!State.IsValid()||!State->TryGetNumberField(TEXT("version"),Version)||Version!=1)return false;
    FClaraRoutingLab Restored;
    for(const FString& Id:{FString(TEXT("R1")),FString(TEXT("R2"))})
    {
        const TSharedPtr<FJsonObject>* O=nullptr;const TSharedPtr<FJsonObject>* Running=nullptr;const TSharedPtr<FJsonObject>* Startup=nullptr;
        FRouter* R=Restored.Router(Id);int32 Mode=0;
        if(!State->TryGetObjectField(Id,O)||!O->IsValid()||!(*O)->TryGetObjectField(TEXT("running"),Running)||!(*O)->TryGetObjectField(TEXT("startup"),Startup)||
           !ReadConfig(*Running,R->Running)||!ReadConfig(*Startup,R->Startup)||!(*O)->TryGetNumberField(TEXT("mode"),Mode)||Mode<0||Mode>static_cast<int32>(EMode::Console)||
           !(*O)->TryGetNumberField(TEXT("interface"),R->Interface)||R->Interface<0||R->Interface>1||
           !(*O)->TryGetBoolField(TEXT("awaitingSave"),R->AwaitingSave)||!(*O)->TryGetBoolField(TEXT("awaitingReload"),R->AwaitingReload)||
           (R->AwaitingSave&&R->AwaitingReload))return false;
        R->Mode=static_cast<EMode>(Mode);
    }
    const TSharedPtr<FJsonObject>* Switch=nullptr;
    if(!State->TryGetObjectField(TEXT("SW2"),Switch)||!Restored.SW2.ImportState(*Switch))return false;
    const TArray<TSharedPtr<FJsonValue>>* Links=nullptr;if(!State->TryGetArrayField(TEXT("cables"),Links)||Links->Num()>4)return false;
    Restored.Cables.Empty();
    for(const auto& Link:*Links)
    {
        FString EdgeValue,A,B;if(!Link.IsValid()||!Link->TryGetString(EdgeValue)||!EdgeValue.Split(TEXT("|"),&A,&B))return false;
        Restored.SetCable(A,B,true);if(!Restored.HasCable(A,B))return false;
    }
    *this=MoveTemp(Restored);return true;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
namespace
{
void Commands(FClaraRoutingLab& Lab,const TCHAR* Device,std::initializer_list<const TCHAR*> Lines)
{for(const TCHAR* Line:Lines)Lab.Execute(Device,Line);}
void ConfigureRoutingBase(FClaraRoutingLab& Lab)
{
    Lab.SetCable(TEXT("R1"),TEXT("R2"),true);Lab.SetCable(TEXT("R2"),TEXT("SW2"),true);
    Commands(Lab,TEXT("R1"),{TEXT("enable"),TEXT("configure terminal"),TEXT("interface gigabitethernet 0/1"),TEXT("ip address 10.0.0.1 255.255.255.252"),TEXT("no shutdown"),TEXT("end"),TEXT("write memory")});
    Commands(Lab,TEXT("R2"),{TEXT("enable"),TEXT("configure terminal"),TEXT("hostname R2"),TEXT("enable secret Cisco123"),TEXT("line console 0"),TEXT("password Console123"),TEXT("login"),TEXT("exit"),TEXT("interface gigabitethernet 0/0"),TEXT("ip address 10.0.0.2 255.255.255.252"),TEXT("no shutdown"),TEXT("exit"),TEXT("interface gigabitethernet 0/1"),TEXT("ip address 192.168.2.1 255.255.255.0"),TEXT("no shutdown"),TEXT("end"),TEXT("write memory")});
    Commands(Lab,TEXT("SW2"),{TEXT("enable"),TEXT("configure terminal"),TEXT("hostname SW2"),TEXT("enable secret Cisco123"),TEXT("line console 0"),TEXT("password Console123"),TEXT("login"),TEXT("exit"),TEXT("interface vlan 1"),TEXT("ip address 192.168.2.2 255.255.255.0"),TEXT("no shutdown"),TEXT("exit"),TEXT("ip default-gateway 192.168.2.1"),TEXT("end"),TEXT("write memory")});
}
void ConfigureRoutes(FClaraRoutingLab& Lab,bool Save)
{
    Commands(Lab,TEXT("R1"),{TEXT("enable"),TEXT("end"),TEXT("configure terminal"),TEXT("ip route 192.168.2.0 255.255.255.0 10.0.0.2"),TEXT("end")});
    Commands(Lab,TEXT("R2"),{TEXT("enable"),TEXT("end"),TEXT("configure terminal"),TEXT("ip route 0.0.0.0 0.0.0.0 10.0.0.1"),TEXT("end")});
    if(Save){Lab.Execute(TEXT("R1"),TEXT("write memory"));Lab.Execute(TEXT("R2"),TEXT("write memory"));}
}
void RepairFaults(FClaraRoutingLab& Lab,bool Save)
{
    ConfigureRoutes(Lab,false);
    Commands(Lab,TEXT("R2"),{TEXT("configure terminal"),TEXT("interface gi0/1"),TEXT("no shutdown"),TEXT("end")});
    Commands(Lab,TEXT("SW2"),{TEXT("enable"),TEXT("end"),TEXT("configure terminal"),TEXT("ip default-gateway 192.168.2.1"),TEXT("end")});
    if(Save)for(const TCHAR* Id:{TEXT("R1"),TEXT("R2"),TEXT("SW2")})Lab.Execute(Id,TEXT("write memory"));
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraRoutingCommandsTest,"Clara.Routing.DayThree.CommandValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraRoutingCommandsTest::RunTest(const FString& Parameters)
{
    FClaraRoutingLab Lab;
    TestTrue(TEXT("Day reset retains only network A"),Lab.Ping(TEXT("PC2"),TEXT("192.168.1.1")).StartsWith(TEXT("Reply from")));
    TestFalse(TEXT("New rack starts disconnected"),Lab.HasCable(TEXT("R1"),TEXT("R2")));
    Lab.SetCable(TEXT("LAPTOP"),TEXT("R1"),true);TestFalse(TEXT("Laptop is never a network node"),Lab.HasCable(TEXT("LAPTOP"),TEXT("R1")));
    TestTrue(TEXT("Router configuration requires config mode"),Lab.Execute(TEXT("R1"),TEXT("ip route 0.0.0.0 0.0.0.0 10.0.0.2")).StartsWith(TEXT("%")));
    Commands(Lab,TEXT("R1"),{TEXT("en"),TEXT("conf t"),TEXT("int gi0/1")});
    for(const TCHAR* Bad:{TEXT("ip address 10.0.0.3 255.255.255.252"),TEXT("ip address 10.0.0.0 255.255.255.252"),TEXT("ip address 10.0.0.1 255.0.255.0"),TEXT("ip address 10.0.0.999 255.255.255.0"),TEXT("ip address 010.0.0.1 255.255.255.0"),TEXT("ip address 192.168.1.3 255.255.255.0")})
        TestTrue(FString(TEXT("Reject invalid interface: "))+Bad,Lab.Execute(TEXT("R1"),Bad).StartsWith(TEXT("%")));
    TestTrue(TEXT("Malformed IPv4 rejected"),Lab.Ping(TEXT("PC2"),TEXT("1.2.3")).StartsWith(TEXT("%")));
    TestTrue(TEXT("Multiple commands cannot be injected"),Lab.Execute(TEXT("R1"),TEXT("end\nreload")).StartsWith(TEXT("%")));
    ConfigureRoutingBase(Lab);
    Commands(Lab,TEXT("R1"),{TEXT("end"),TEXT("conf t")});
    for(const TCHAR* Bad:{TEXT("ip route 192.168.2.0 255.255.255.0 10.0.0.3"),TEXT("ip route 192.168.2.0 255.255.255.0 10.0.0.0"),TEXT("ip route 192.168.2.0 255.255.255.0 10.0.0.1"),TEXT("ip route 192.168.2.0 255.255.255.0 172.16.0.1"),TEXT("ip route 192.168.2.9 255.255.255.0 10.0.0.2")})
        TestTrue(FString(TEXT("Reject invalid static route: "))+Bad,Lab.Execute(TEXT("R1"),Bad).StartsWith(TEXT("%")));
    TestTrue(TEXT("Laptop directs student to PC2"),Lab.Execute(TEXT("LAPTOP"),TEXT("ping 192.168.2.2")).Contains(TEXT("solo alla console")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraRoutingPathsTest,"Clara.Routing.DayThree.PathsAndLongestPrefix",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraRoutingPathsTest::RunTest(const FString& Parameters)
{
    FClaraRoutingLab Lab;ConfigureRoutingBase(Lab);
    TestTrue(TEXT("Base config matches script without SSH requirement"),Lab.IsBaseConfigured(true));
    TestTrue(TEXT("No forward route is unreachable"),Lab.Ping(TEXT("PC2"),TEXT("192.168.2.2")).Contains(TEXT("Destination host unreachable")));
    TestFalse(TEXT("Only connected/local routes initially"),Lab.Execute(TEXT("R2"),TEXT("show ip route")).Contains(TEXT("S* 0.0.0.0")));
    Commands(Lab,TEXT("R1"),{TEXT("conf t"),TEXT("ip route 192.168.2.0 255.255.255.0 10.0.0.2"),TEXT("end")});
    TestTrue(TEXT("Forward-only route times out on return"),Lab.Ping(TEXT("PC2"),TEXT("192.168.2.2")).Contains(TEXT("Request timed out")));
    const FString BrokenTrace=Lab.Traceroute(TEXT("PC2"),TEXT("192.168.2.2"));
    TestTrue(TEXT("No return route hides second hop"),BrokenTrace.Contains(TEXT("1  <1 ms  192.168.1.1"))&&BrokenTrace.Contains(TEXT("2  * * *")));
    ConfigureRoutes(Lab,true);
    TestTrue(TEXT("Both directions work"),Lab.Ping(TEXT("PC2"),TEXT("192.168.2.2")).StartsWith(TEXT("Reply from")));
    TestTrue(TEXT("Traceroute contains all three expected hops"),Lab.Traceroute(TEXT("PC2"),TEXT("192.168.2.2")).Contains(TEXT("3  <1 ms  192.168.2.2")));
    TestTrue(TEXT("Default route is marked S*"),Lab.Execute(TEXT("R2"),TEXT("show ip route")).Contains(TEXT("S* 0.0.0.0/0 via 10.0.0.1")));
    Lab.SetCable(TEXT("R1"),TEXT("R2"),false);
    TestFalse(TEXT("Cable state gates traffic"),Lab.Ping(TEXT("PC2"),TEXT("192.168.2.2")).StartsWith(TEXT("Reply from")));
    Lab.SetCable(TEXT("R1"),TEXT("R2"),true);
    Commands(Lab,TEXT("R2"),{TEXT("conf t"),TEXT("int g0/1"),TEXT("shutdown"),TEXT("end")});
    const FString DownTrace=Lab.Traceroute(TEXT("PC2"),TEXT("192.168.2.2"));
    TestTrue(TEXT("Down LAN stops after R2"),DownTrace.Contains(TEXT("2  <1 ms  10.0.0.2"))&&DownTrace.Contains(TEXT("3  * * *")));
    Commands(Lab,TEXT("R2"),{TEXT("conf t"),TEXT("int g0/1"),TEXT("no shut"),TEXT("end")});
    Commands(Lab,TEXT("SW2"),{TEXT("conf t"),TEXT("ip default-gateway 192.168.1.1"),TEXT("end")});
    TestTrue(TEXT("Wrong gateway breaks return"),Lab.Ping(TEXT("PC2"),TEXT("192.168.2.2")).Contains(TEXT("Request timed out")));
    Commands(Lab,TEXT("SW2"),{TEXT("conf t"),TEXT("ip default-gateway 192.168.2.1"),TEXT("end")});
    Commands(Lab,TEXT("R1"),{TEXT("conf t"),TEXT("ip route 192.168.0.0 255.255.0.0 192.168.1.12"),TEXT("end")});
    TestTrue(TEXT("Specific /24 wins over unreachable /16"),Lab.Ping(TEXT("PC2"),TEXT("192.168.2.2")).StartsWith(TEXT("Reply from")));
    Commands(Lab,TEXT("R1"),{TEXT("conf t"),TEXT("ip route 192.168.2.2 255.255.255.255 192.168.1.12"),TEXT("end")});
    TestTrue(TEXT("More specific /32 overrides /24 and fails"),Lab.Ping(TEXT("PC2"),TEXT("192.168.2.2")).Contains(TEXT("Destination host unreachable")));
    Commands(Lab,TEXT("R1"),{TEXT("conf t"),TEXT("no ip route 192.168.2.2 255.255.255.255 192.168.1.12"),TEXT("end")});
    TestTrue(TEXT("Removing specific route restores reachability"),Lab.Ping(TEXT("PC2"),TEXT("192.168.2.2")).StartsWith(TEXT("Reply from")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraRoutingPersistenceTest,"Clara.Routing.DayThree.FaultsAndPersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraRoutingPersistenceTest::RunTest(const FString& Parameters)
{
    FClaraRoutingLab Lab;ConfigureRoutingBase(Lab);ConfigureRoutes(Lab,true);
    TestTrue(TEXT("Complete configured lab passes Gaia"),Lab.Validate(true).IsEmpty());
    Lab.InjectFaults();TestEqual(TEXT("Injecting faults does not reset an open console session"),Lab.GetPrompt(TEXT("R1")),FString(TEXT("R1# ")));Lab.Reboot();
    TestTrue(TEXT("Faulty broadcast route is persisted deliberately"),Lab.HasRoute(TEXT("R1"),TEXT("192.168.2.0"),TEXT("255.255.255.0"),TEXT("10.0.0.3")));
    TestFalse(TEXT("Faulty startup has no return route"),Lab.HasRoute(TEXT("R2"),TEXT("0.0.0.0"),TEXT("0.0.0.0"),TEXT("10.0.0.1")));
    TestFalse(TEXT("R2 shutdown persists"),Lab.IsDeviceConfigured(TEXT("R2"),false));
    TestFalse(TEXT("SW2 wrong gateway persists"),Lab.IsDeviceConfigured(TEXT("SW2"),false));
    TestFalse(TEXT("Faulty state fails Gaia"),Lab.Validate(true).IsEmpty());
    RepairFaults(Lab,false);
    TestTrue(TEXT("Repair works before save"),Lab.Validate(false).IsEmpty());
    TestFalse(TEXT("Gaia also requires all startup snapshots"),Lab.Validate(true).IsEmpty());
    FClaraRoutingLab Loaded;TestTrue(TEXT("Checkpoint round trip"),Loaded.ImportState(Lab.ExportState()));
    TestTrue(TEXT("Checkpoint keeps unsaved operational changes"),Loaded.Validate(false).IsEmpty());
    TestFalse(TEXT("App save is not IOS save"),Loaded.IsStartupSaved(TEXT("R1")));
    Loaded.Reboot();TestFalse(TEXT("Reload discards unsaved repairs"),Loaded.Validate(false).IsEmpty());
    RepairFaults(Loaded,true);Loaded.Reboot();TestTrue(TEXT("Saved repairs survive reboot"),Loaded.Validate(true).IsEmpty());
    auto Invalid=Loaded.ExportState();Invalid->GetObjectField(TEXT("R2"))->SetNumberField(TEXT("interface"),9);
    TestFalse(TEXT("Malformed checkpoint rejected"),Loaded.ImportState(Invalid));
    TestTrue(TEXT("Rejected checkpoint leaves current lab intact"),Loaded.Validate(true).IsEmpty());
    Commands(Loaded,TEXT("R1"),{TEXT("enable"),TEXT("conf t"),TEXT("hostname TEMP"),TEXT("end"),TEXT("copy running-config startup-config")});
    FClaraRoutingLab Pending;TestTrue(TEXT("Pending copy snapshot imported"),Pending.ImportState(Loaded.ExportState()));
    TestFalse(TEXT("Pending save is not saved"),Pending.IsStartupSaved(TEXT("R1")));
    Pending.Execute(TEXT("R1"),TEXT(""));TestTrue(TEXT("Explicit confirmation writes startup"),Pending.IsStartupSaved(TEXT("R1")));
    return true;
}
#endif
