#include "ClaraSwitchCLI.h"

namespace
{
TArray<FString> Tokens(const FString& Input)
{
    TArray<FString> Words;
    Input.ParseIntoArrayWS(Words);
    // Both `interface vlan 10` and `interface Vlan10` select an SVI.
    if (Words.Num() >= 2 && !Words[0].IsEmpty() && FString(TEXT("interface")).StartsWith(Words[0], ESearchCase::IgnoreCase))
    {
        int32 FirstDigit = INDEX_NONE;
        for (int32 Index = 0; Index < Words[1].Len(); ++Index)
            if (Words[1][Index] >= TEXT('0') && Words[1][Index] <= TEXT('9')) { FirstDigit = Index; break; }
        if (FirstDigit > 0 && FString(TEXT("vlan")).StartsWith(Words[1].Left(FirstDigit), ESearchCase::IgnoreCase))
        {
            const FString Number = Words[1].Mid(FirstDigit);
            Words[1] = TEXT("vlan");
            Words.Insert(Number, 2);
        }
    }
    int32 Tail = INDEX_NONE;
    if (Words.Num() >= 3 && FString(TEXT("banner")).StartsWith(Words[0], ESearchCase::IgnoreCase)) Tail = 2;
    if (Words.Num() >= 3 && Words[0].Equals(TEXT("enable"), ESearchCase::IgnoreCase) && Words[1].Equals(TEXT("secret"), ESearchCase::IgnoreCase)) Tail = 2;
    if (Words.Num() >= 2 && FString(TEXT("password")).StartsWith(Words[0], ESearchCase::IgnoreCase)) Tail = 1;
    if (Words.Num() >= 4 && FString(TEXT("username")).StartsWith(Words[0], ESearchCase::IgnoreCase)) Tail = 3;
    if (Tail != INDEX_NONE && Words.Num() > Tail+1)
    {
        TArray<FString> Rest;
        for(int32 I=Tail;I<Words.Num();++I)Rest.Add(Words[I]);
        Words.SetNum(Tail);Words.Add(FString::Join(Rest,TEXT(" ")));
    }
    return Words;
}

bool ParseNumber(const FString& Value, int32 Minimum, int32 Maximum, int32& Number)
{
    if (Value.IsEmpty() || Value.Len() > 5) return false;
    for (TCHAR Char : Value) if (Char < TEXT('0') || Char > TEXT('9')) return false;
    Number = FCString::Atoi(*Value);
    return Number >= Minimum && Number <= Maximum;
}

bool ParseIPv4(const FString& Value, uint32& Address)
{
    TArray<FString> Octets;
    Value.ParseIntoArray(Octets, TEXT("."), false);
    if (Octets.Num() != 4) return false;
    Address = 0;
    for (const FString& Octet : Octets)
    {
        int32 Number = 0;
        if (Octet.Len() > 3 || !ParseNumber(Octet, 0, 255, Number) || (Octet.Len() > 1 && Octet[0] == TEXT('0'))) return false;
        Address = (Address << 8) | static_cast<uint32>(Number);
    }
    return true;
}

FString InvalidAt(const FString& Input, int32 Word, const FString& Detail = FString())
{
    int32 Position = 0;
    for (int32 Index = 0; Index <= Word && Position < Input.Len(); ++Index)
    {
        while (Position < Input.Len() && FChar::IsWhitespace(Input[Position])) ++Position;
        if (Index == Word) break;
        while (Position < Input.Len() && !FChar::IsWhitespace(Input[Position])) ++Position;
    }
    return Input + TEXT("\n") + FString::ChrN(Position, TEXT(' ')) + TEXT("^\n% Invalid input detected at '^' marker.")
        + (Detail.IsEmpty() ? TEXT("\nQuesto comando non e' incluso nel sottoinsieme locale. Usa ? nel contesto corrente.") : TEXT("\n") + Detail);
}

FString Description(const FString& Word)
{
    if (Word == TEXT("enable")) return TEXT("Entra nella modalita' EXEC privilegiata");
    if (Word == TEXT("disable")) return TEXT("Torna alla modalita' EXEC utente");
    if (Word == TEXT("configure")) return TEXT("Entra nella configurazione");
    if (Word == TEXT("terminal")) return TEXT("Configura dalla console corrente");
    if (Word == TEXT("hostname")) return TEXT("Assegna un nome al dispositivo");
    if (Word == TEXT("interface")) return TEXT("Seleziona un'interfaccia VLAN (SVI)");
    if (Word == TEXT("vlan")) return TEXT("Seleziona o crea una VLAN");
    if (Word == TEXT("ip")) return TEXT("Parametri e informazioni IPv4");
    if (Word == TEXT("address")) return TEXT("Imposta indirizzo e subnet mask");
    if (Word == TEXT("show")) return TEXT("Osserva lo stato senza modificarlo");
    if (Word == TEXT("running-config")) return TEXT("Configurazione attualmente in uso");
    if (Word == TEXT("startup-config")) return TEXT("Copia salvata, separata da quella in uso");
    if (Word == TEXT("copy")) return TEXT("Copia la configurazione in uso in quella salvata");
    if (Word == TEXT("write")) return TEXT("Salva la configurazione in uso");
    if (Word == TEXT("memory")) return TEXT("Scrive la copia startup del modello locale");
    if (Word == TEXT("shutdown")) return TEXT("Disabilita amministrativamente l'interfaccia");
    if (Word == TEXT("no")) return TEXT("Rimuove l'impostazione shutdown supportata");
    if (Word == TEXT("exit")) return TEXT("Torna al livello di configurazione precedente");
    if (Word == TEXT("end")) return TEXT("Torna direttamente a EXEC privilegiato");
    if (Word == TEXT("brief")) return TEXT("Mostra un riepilogo");
    if (Word == TEXT("help")) return TEXT("Spiega gli aiuti del terminale locale");
    if (Word == TEXT("$hostname")) return TEXT("Nome: 1-63 caratteri, iniziale alfabetica, lettere/cifre/trattini");
    if (Word == TEXT("$vlan")) return TEXT("Identificativo VLAN: 1-4094");
    if (Word == TEXT("$ip")) return TEXT("Indirizzo IPv4 unicast dell'interfaccia");
    if (Word == TEXT("$mask")) return TEXT("Subnet mask IPv4 con bit contigui");
    if(Word==TEXT("$secret"))return TEXT("Segreto conservato nel modello locale; hash Cisco non simulato");
    if(Word==TEXT("$bits"))return TEXT("Dimensione RSA; almeno 2048 bit per il giorno 2");
    if(Word==TEXT("$transport"))return TEXT("ssh, telnet, all o none; questo incarico richiede solo ssh");
    if(Word==TEXT("do"))return TEXT("Esegue una verifica EXEC dalla configurazione");
    if (Word == TEXT("<cr>")) return TEXT("Premi Invio per eseguire");
    return FString();
}

FString DisplayWord(const FString& Word)
{
    if (Word == TEXT("$hostname")||Word==TEXT("$user")||Word==TEXT("$secret")||Word==TEXT("$domain")||Word==TEXT("$text")) return TEXT("WORD");
    if(Word==TEXT("$bits"))return TEXT("<512-4096>");
    if (Word == TEXT("$vlan")) return TEXT("<1-4094>");
    if (Word == TEXT("$ip") || Word == TEXT("$mask")) return TEXT("A.B.C.D");
    return Word;
}
}

FClaraSwitchCLI::FClaraSwitchCLI() { Reset(); }

void FClaraSwitchCLI::Reset()
{
    Running = FConfiguration();
    Running.Vlans.Add(1);
    Running.Interfaces.Add(1, FInterface());
    Startup = Running;
    Mode = EMode::UserExec;
    CurrentInterface = 1;
    bAwaitingSaveConfirmation = false;
    bAwaitingRSA=false;bAwaitingBanner=false;bAwaitingReload=false;BannerDelimiter=0;PendingBanner.Empty();
    LearnedMacTable.Empty();ReachableTargets.Empty();
}

FString FClaraSwitchCLI::GetPrompt() const
{
    if (bAwaitingSaveConfirmation) return TEXT("Destination filename [startup-config]? ");
    if (bAwaitingRSA) return TEXT("How many bits in the modulus [2048]: ");
    if (bAwaitingBanner) return FString();
    if (bAwaitingReload) return TEXT("Proceed with reload? [confirm] ");
    switch (Mode)
    {
    case EMode::UserExec: return Running.Hostname + TEXT(">");
    case EMode::PrivilegedExec: return Running.Hostname + TEXT("#");
    case EMode::GlobalConfig: return Running.Hostname + TEXT("(config)#");
    case EMode::VlanConfig: return Running.Hostname + TEXT("(config-vlan)#");
    case EMode::InterfaceConfig: return Running.Hostname + TEXT("(config-if)#");
    case EMode::LineConsole:
    case EMode::LineVTY: return Running.Hostname + TEXT("(config-line)#");
    }
    return Running.Hostname + TEXT(">");
}

TArray<FClaraSwitchCLI::FRule> FClaraSwitchCLI::GetRules() const
{
    TArray<FRule> Rules;
    auto Add = [&Rules](const TCHAR* Pattern, const TCHAR* Action) { Rules.Add({Pattern, Action}); };
    Add(TEXT("help"), TEXT("help"));
    if (Mode == EMode::UserExec || Mode == EMode::PrivilegedExec)
    {
        Add(TEXT("enable"), TEXT("enable"));
        Add(TEXT("show ip interface brief"), TEXT("show_interfaces"));
        Add(TEXT("show vlan brief"), TEXT("show_vlans"));
        Add(TEXT("show mac address-table"), TEXT("show_mac"));
        Add(TEXT("show mac-address-table"), TEXT("show_mac"));
        Add(TEXT("ping $ip"), TEXT("ping"));
    }
    if (Mode == EMode::PrivilegedExec)
    {
        Add(TEXT("disable"), TEXT("disable"));
        Add(TEXT("configure terminal"), TEXT("configure"));
        Add(TEXT("show running-config"), TEXT("show_running"));
        Add(TEXT("show startup-config"), TEXT("show_startup"));
        Add(TEXT("copy running-config startup-config"), TEXT("copy"));
        Add(TEXT("write memory"), TEXT("save"));
        Add(TEXT("write"), TEXT("save"));
        Add(TEXT("reload"), TEXT("reload"));
    }
    if (Mode == EMode::GlobalConfig)
    {
        Add(TEXT("hostname $hostname"), TEXT("hostname"));
        Add(TEXT("vlan $vlan"), TEXT("vlan"));
        Add(TEXT("interface vlan $vlan"), TEXT("interface"));
        Add(TEXT("enable secret $secret"), TEXT("enable_secret"));
        Add(TEXT("line console 0"), TEXT("line_console"));
        Add(TEXT("line vty 0 15"), TEXT("line_vty"));
        Add(TEXT("ip domain-name $domain"), TEXT("domain"));
        Add(TEXT("ip default-gateway $ip"), TEXT("gateway"));
        Add(TEXT("ip ssh version 2"), TEXT("ssh_version"));
        Add(TEXT("crypto key generate rsa"), TEXT("rsa_prompt"));
        Add(TEXT("crypto key generate rsa modulus $bits"), TEXT("rsa_modulus"));
        Add(TEXT("username $user secret $secret"), TEXT("username"));
        Add(TEXT("banner motd"), TEXT("banner_prompt"));
        Add(TEXT("banner motd $text"), TEXT("banner"));
        Add(TEXT("service password-encryption"), TEXT("password_encryption"));
        Add(TEXT("no service password-encryption"), TEXT("no_password_encryption"));
    }
    if (Mode == EMode::InterfaceConfig)
    {
        Add(TEXT("ip address $ip $mask"), TEXT("address"));
        Add(TEXT("shutdown"), TEXT("shutdown"));
        Add(TEXT("no shutdown"), TEXT("no_shutdown"));
    }
    if (Mode == EMode::LineConsole || Mode == EMode::LineVTY)
    {
        Add(TEXT("password $secret"),TEXT("line_password"));
        Add(TEXT("login"),TEXT("line_login"));
        Add(TEXT("login local"),TEXT("line_login_local"));
        if(Mode==EMode::LineVTY)
        {
            Add(TEXT("transport input ssh"),TEXT("transport"));
            Add(TEXT("transport input telnet"),TEXT("transport"));
            Add(TEXT("transport input all"),TEXT("transport"));
            Add(TEXT("transport input none"),TEXT("transport"));
        }
    }
    if (Mode == EMode::GlobalConfig || Mode == EMode::VlanConfig || Mode == EMode::InterfaceConfig || Mode == EMode::LineConsole || Mode == EMode::LineVTY)
    {
        Add(TEXT("do show running-config"),TEXT("show_running"));
        Add(TEXT("do show startup-config"),TEXT("show_startup"));
        Add(TEXT("do show ip interface brief"),TEXT("show_interfaces"));
        Add(TEXT("do show vlan brief"),TEXT("show_vlans"));
        Add(TEXT("do show mac address-table"),TEXT("show_mac"));
        Add(TEXT("do ping $ip"),TEXT("ping"));
        Add(TEXT("exit"), TEXT("exit"));
        Add(TEXT("end"), TEXT("end"));
    }
    return Rules;
}

FClaraSwitchCLI::FResolution FClaraSwitchCLI::Resolve(const TArray<FString>& Words, const FString& Original) const
{
    FResolution Result;
    Result.Rules = GetRules();
    for (int32 Index = 0; Index < Words.Num(); ++Index)
    {
        TArray<FRule> Matching;
        TArray<FString> Options;
        bool bExactKeyword = false;
        for (const FRule& Rule : Result.Rules)
        {
            const TArray<FString> Pattern = Tokens(Rule.Pattern);
            if (!Pattern.IsValidIndex(Index)) continue;
            const FString& Expected = Pattern[Index];
            if (Expected.StartsWith(TEXT("$")) || Expected.StartsWith(Words[Index], ESearchCase::IgnoreCase))
            {
                Matching.Add(Rule);
                Options.AddUnique(Expected);
                if (!Expected.StartsWith(TEXT("$")) && Expected.Equals(Words[Index], ESearchCase::IgnoreCase)) bExactKeyword = true;
            }
        }
        if (Matching.IsEmpty())
        {
            Result.Error = InvalidAt(Original, Index);
            return Result;
        }
        if (bExactKeyword)
        {
            Options.RemoveAll([&](const FString& Option) { return !Option.Equals(Words[Index], ESearchCase::IgnoreCase); });
            Matching.RemoveAll([&](const FRule& Rule) { return !Tokens(Rule.Pattern)[Index].Equals(Words[Index], ESearchCase::IgnoreCase); });
        }
        if (Options.Num() > 1)
        {
            Result.Error = FString::Printf(TEXT("%% Ambiguous command: \"%s\"\nUsa ? per vedere le alternative."), *Words[Index]);
            return Result;
        }
        Result.Canonical.Add(Options[0].StartsWith(TEXT("$")) ? Words[Index] : Options[0]);
        Result.Rules = MoveTemp(Matching);
    }
    return Result;
}

TArray<FString> FClaraSwitchCLI::GetSuggestions(const FString& Prefix, TArray<FString>& Canonical, bool& bTrailingSpace, FString& Partial) const
{
    TArray<FString> Suggestions;
    if (Prefix.Len() > 256) return Suggestions;
    FString Input = Prefix;
    int32 Question = INDEX_NONE;
    if (Input.FindChar(TEXT('?'), Question)) Input.LeftInline(Question);
    bTrailingSpace = !Input.IsEmpty() && FChar::IsWhitespace(Input[Input.Len() - 1]);
    TArray<FString> Words = Tokens(Input);
    Partial.Empty();
    if (!bTrailingSpace && !Words.IsEmpty()) Partial = Words.Pop();
    const FResolution Resolution = Resolve(Words, Input);
    if (!Resolution.Error.IsEmpty()) return Suggestions;
    Canonical = Resolution.Canonical;
    for (const FRule& Rule : Resolution.Rules)
    {
        const TArray<FString> Pattern = Tokens(Rule.Pattern);
        if (Pattern.IsValidIndex(Words.Num()))
        {
            const FString& Option = Pattern[Words.Num()];
            // FString::StartsWith("") is false: an empty current word must expose all next options.
            if (Partial.IsEmpty() || Option.StartsWith(TEXT("$")) || Option.StartsWith(Partial, ESearchCase::IgnoreCase)) Suggestions.AddUnique(Option);
        }
        else if (Partial.IsEmpty()) Suggestions.AddUnique(TEXT("<cr>"));
    }
    Suggestions.Sort();
    return Suggestions;
}

FString FClaraSwitchCLI::Help(const FString& Prefix) const
{
    if (bAwaitingSaveConfirmation) return TEXT("  <cr>  Conferma il nome startup-config e salva\n  startup-config  Nome supportato da questo esercizio locale");
    if(bAwaitingRSA)return TEXT("  512  768  1024  2048  3072  4096\n  <cr>  Usa 2048 bit nel modello locale");
    if(bAwaitingBanner)return TEXT("  Ripeti il delimitatore scelto per terminare il banner.");
    if(bAwaitingReload)return TEXT("  <cr>  Ripristina startup-config e riavvia il modello\n  no    Annulla");
    TArray<FString> Canonical;
    bool bTrailing = false;
    FString Partial;
    const TArray<FString> Suggestions = GetSuggestions(Prefix, Canonical, bTrailing, Partial);
    if (Suggestions.IsEmpty()) return TEXT("% Nessun aiuto per questo prefisso nel contesto corrente. Usa ? su una riga vuota.");
    FString Output;
    for (const FString& Option : Suggestions)
        Output += FString::Printf(TEXT("  %-20s %s\n"), *DisplayWord(Option), *Description(Option));
    return Output.TrimEnd();
}

FString FClaraSwitchCLI::Complete(const FString& Prefix) const
{
    if (bAwaitingSaveConfirmation) return Prefix;
    TArray<FString> Canonical;
    bool bTrailing = false;
    FString Partial;
    TArray<FString> Suggestions = GetSuggestions(Prefix, Canonical, bTrailing, Partial);
    Suggestions.RemoveAll([](const FString& Option) { return Option.StartsWith(TEXT("$")) || Option == TEXT("<cr>"); });
    if (Suggestions.IsEmpty()) return Prefix;
    FString Common = Suggestions[0];
    for (int32 Index = 1; Index < Suggestions.Num(); ++Index)
        while (!Common.IsEmpty() && !Suggestions[Index].StartsWith(Common, ESearchCase::IgnoreCase)) Common.LeftChopInline(1);
    if (Common.IsEmpty()) return Prefix;
    Canonical.Add(Common);
    FString Result = FString::Join(Canonical, TEXT(" "));
    if (Suggestions.Num() == 1) Result += TEXT(" ");
    return Result;
}

FString FClaraSwitchCLI::Execute(const FString& Input)
{
    const FString Clean = Input.TrimStartAndEnd();
    if (bAwaitingSaveConfirmation)
    {
        if (Clean.IsEmpty() || Clean.Equals(TEXT("startup-config"), ESearchCase::IgnoreCase)) return SaveStartup();
        if (Clean.Contains(TEXT("?"))) return Help(Clean);
        return TEXT("% Questo esercizio salva soltanto startup-config. Premi Invio per confermare il nome proposto; la copia non e' ancora stata salvata.");
    }
    if (bAwaitingRSA)
    {
        if(Clean==TEXT("?"))return TEXT("  512  768  1024  2048  3072  4096\nPer il giorno 2 sono richiesti almeno 2048 bit.");
        int32 Bits=2048;
        if(!Clean.IsEmpty()&&!ParseNumber(Clean,512,4096,Bits))return TEXT("% Dimensione non valida. Scegli 512, 768, 1024, 2048, 3072 o 4096.");
        return GenerateRSA(Bits);
    }
    if(bAwaitingBanner)
    {
        FString Line=Input;
        if(!BannerDelimiter)
        {
            if(Line.IsEmpty())return TEXT("Indica un delimitatore, poi il messaggio; ripeti il delimitatore per terminare.");
            BannerDelimiter=Line[0];Line.RightChopInline(1);
        }
        int32 End=INDEX_NONE;
        if(Line.FindChar(BannerDelimiter,End))
        {
            PendingBanner+=Line.Left(End);Running.Banner=PendingBanner;bAwaitingBanner=false;BannerDelimiter=0;PendingBanner.Empty();return FString();
        }
        PendingBanner+=Line+TEXT("\n");
        return TEXT("Messaggio in corso: termina con il delimitatore scelto.");
    }
    if(bAwaitingReload)
    {
        if(Clean.Equals(TEXT("no"),ESearchCase::IgnoreCase)||Clean.Equals(TEXT("cancel"),ESearchCase::IgnoreCase))
        {bAwaitingReload=false;return TEXT("Reload annullato.");}
        if(!Clean.IsEmpty()&&!Clean.Equals(TEXT("yes"),ESearchCase::IgnoreCase))return TEXT("Premi Invio per confermare, oppure scrivi no per annullare.");
        ReloadFromStartup();
        return TEXT("Riavvio del modello locale: startup-config ripristinata e tabella MAC dinamica svuotata. Nessun apparato esterno riavviato.");
    }
    if (Clean.IsEmpty()) return FString();
    if (Clean.Len() > 256 || Clean.Contains(TEXT("\n")) || Clean.Contains(TEXT("\r")))
        return TEXT("% Inserisci un solo comando di massimo 256 caratteri.");
    int32 Question = INDEX_NONE;
    if (Input.FindChar(TEXT('?'), Question)) return Help(Input.Left(Question));
    const TArray<FString> Words = Tokens(Clean);
    const FResolution Resolution = Resolve(Words, Clean);
    if (!Resolution.Error.IsEmpty()) return Resolution.Error;
    for (const FRule& Rule : Resolution.Rules)
        if (Tokens(Rule.Pattern).Num() == Words.Num()) return ApplyRule(Rule, Resolution.Canonical);
    return TEXT("% Incomplete command.\nAggiungi uno spazio e ? per vedere quale parte manca.");
}

FString FClaraSwitchCLI::ApplyRule(const FRule& Rule, const TArray<FString>& Words)
{
    const FString& Action = Rule.Action;
    if (Action == TEXT("help"))
        return TEXT("Sottoinsieme didattico locale: non e' Cisco IOS in esecuzione.\n? mostra l'aiuto nel punto in cui scrivi. TAB completa parole non ambigue.\nI prompt >, #, (config)# e (config-if)# indicano contesti diversi.\nI comandi non implementati sono rifiutati, non simulati come riusciti.\nLa copia startup e' distinta dalla configurazione in uso; il reset del tutor ripristina entrambe.");
    if (Action == TEXT("enable")) { Mode = EMode::PrivilegedExec; return FString(); }
    if (Action == TEXT("disable")) { Mode = EMode::UserExec; return FString(); }
    if (Action == TEXT("configure")) { Mode = EMode::GlobalConfig; return TEXT("Enter configuration commands, one per line. End with end."); }
    if (Action == TEXT("end")) { Mode = EMode::PrivilegedExec; return FString(); }
    if (Action == TEXT("exit"))
    {
        Mode = Mode == EMode::GlobalConfig ? EMode::PrivilegedExec : EMode::GlobalConfig;
        return FString();
    }
    if (Action == TEXT("hostname"))
    {
        const FString& Name = Words[1];
        bool Valid = Name.Len() >= 1 && Name.Len() <= 63;
        auto IsLetter = [](TCHAR Char) { return (Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('A') && Char <= TEXT('Z')); };
        auto IsDigit = [](TCHAR Char) { return Char >= TEXT('0') && Char <= TEXT('9'); };
        Valid = Valid && IsLetter(Name[0]) && (IsLetter(Name[Name.Len() - 1]) || IsDigit(Name[Name.Len() - 1]));
        for (TCHAR Char : Name) Valid = Valid && (IsLetter(Char) || IsDigit(Char) || Char == TEXT('-'));
        if (!Valid) return TEXT("% Nome non valido. Usa 1-63 caratteri: iniziale alfabetica, finale alfanumerica, lettere, cifre e trattini.");
        Running.Hostname = Name;
        return FString();
    }
    if (Action == TEXT("vlan") || Action == TEXT("interface"))
    {
        int32 Id = 0;
        if (!ParseNumber(Words.Last(), 1, 4094, Id)) return TEXT("% VLAN non valida. L'identificativo deve essere un intero da 1 a 4094.");
        if (Action == TEXT("vlan"))
        {
            Running.Vlans.Add(Id);
            Mode = EMode::VlanConfig;
        }
        else
        {
            CurrentInterface = Id;
            Running.Interfaces.FindOrAdd(Id);
            Mode = EMode::InterfaceConfig;
        }
        return FString();
    }
    if (Action == TEXT("address"))
    {
        uint32 Address = 0, Mask = 0;
        if (!ParseIPv4(Words[2], Address)) return InvalidAt(FString::Join(Words, TEXT(" ")), 2, TEXT("Indirizzo IPv4 non valido."));
        if (!ParseIPv4(Words[3], Mask)) return InvalidAt(FString::Join(Words, TEXT(" ")), 3, TEXT("Subnet mask non valida."));
        const uint32 Inverse = ~Mask;
        if (Mask == 0 || (Inverse & (Inverse + 1U)) != 0)
            return TEXT("% Bad mask: i bit a 1 della subnet mask devono essere contigui, seguiti dai bit a 0.");
        const uint32 First = Address >> 24;
        if (First == 0 || First == 127 || First >= 224)
            return TEXT("% Indirizzo non valido per questa SVI: usa un indirizzo IPv4 unicast assegnabile.");
        if (Inverse > 1 && ((Address & Inverse) == 0 || (Address & Inverse) == Inverse))
            return TEXT("% Bad mask for address: hai indicato l'indirizzo di rete o di broadcast, non un host della subnet.");
        for (const auto& Pair : Running.Interfaces)
        {
            if (Pair.Key == CurrentInterface || Pair.Value.Address.IsEmpty()) continue;
            uint32 OtherAddress = 0, OtherMask = 0;
            if (ParseIPv4(Pair.Value.Address, OtherAddress) && ParseIPv4(Pair.Value.Mask, OtherMask)
                && (Address & (Mask & OtherMask)) == (OtherAddress & (Mask & OtherMask)))
                return FString::Printf(TEXT("%% La subnet si sovrappone a Vlan%d. Nessuna modifica applicata."), Pair.Key);
        }
        FInterface& Interface = Running.Interfaces.FindChecked(CurrentInterface);
        Interface.Address = Words[2];
        Interface.Mask = Words[3];
        return FString();
    }
    if (Action == TEXT("shutdown") || Action == TEXT("no_shutdown"))
    {
        Running.Interfaces.FindChecked(CurrentInterface).bShutdown = Action == TEXT("shutdown");
        return FString();
    }
    if(Action==TEXT("enable_secret")){Running.EnableSecret=Words.Last();return FString();}
    if(Action==TEXT("line_console")){Mode=EMode::LineConsole;return FString();}
    if(Action==TEXT("line_vty")){Mode=EMode::LineVTY;return FString();}
    if(Action==TEXT("line_password"))
    {if(Mode==EMode::LineConsole)Running.ConsolePassword=Words.Last();else Running.VTYPassword=Words.Last();return FString();}
    if(Action==TEXT("line_login")||Action==TEXT("line_login_local"))
    {
        const bool Local=Action==TEXT("line_login_local");
        if(Mode==EMode::LineConsole){Running.bConsoleLogin=true;Running.bConsoleLoginLocal=Local;}
        else{Running.bVTYLogin=true;Running.bVTYLoginLocal=Local;}
        return FString();
    }
    if(Action==TEXT("transport"))
    {
        FString Value=Words.Last().ToLower();
        if(Value!=TEXT("ssh")&&Value!=TEXT("telnet")&&Value!=TEXT("all")&&Value!=TEXT("none"))return TEXT("% Trasporto non supportato: ssh, telnet, all o none.");
        Running.Transport=Value;return FString();
    }
    if(Action==TEXT("domain"))
    {
        const FString& Value=Words.Last();
        if(Value.IsEmpty()||Value.Len()>253||Value.Contains(TEXT("/"))||Value.Contains(TEXT("@")))return TEXT("% Nome di dominio non valido.");
        Running.DomainName=Value;return FString();
    }
    if(Action==TEXT("gateway"))
    {
        uint32 Address=0;if(!ParseIPv4(Words.Last(),Address)||(Address>>24)==0||(Address>>24)==127||(Address>>24)>=224)return TEXT("% Gateway IPv4 unicast non valido.");
        Running.Gateway=Words.Last();return FString();
    }
    if(Action==TEXT("ssh_version")){Running.SSHVersion=2;return FString();}
    if(Action==TEXT("rsa_prompt")||Action==TEXT("rsa_modulus"))
    {
        if(Running.Hostname==TEXT("Switch"))return TEXT("% Imposta prima un hostname riconoscibile.");
        if(Running.DomainName.IsEmpty())return TEXT("% Imposta prima ip domain-name per l'identita' SSH.");
        if(Action==TEXT("rsa_prompt")){bAwaitingRSA=true;return TEXT("Dimensione della chiave RSA nel modello locale. Nessuna chiave crittografica reale verra' generata.\nHow many bits in the modulus [2048]:");}
        int32 Bits=0;if(!ParseNumber(Words.Last(),512,4096,Bits))return TEXT("% Dimensione RSA non valida.");return GenerateRSA(Bits);
    }
    if(Action==TEXT("username"))
    {if(Words[1].Len()>63||Words[1].Contains(TEXT(" ")))return TEXT("% Nome utente non valido.");Running.Users.Add(Words[1],Words.Last());return FString();}
    if(Action==TEXT("banner_prompt")||Action==TEXT("banner"))
    {
        bAwaitingBanner=true;BannerDelimiter=0;PendingBanner.Empty();
        if(Action==TEXT("banner"))return Execute(Words.Last());
        return TEXT("Inserisci il delimitatore e il messaggio MOTD; ripeti il delimitatore per terminare.");
    }
    if(Action==TEXT("password_encryption")||Action==TEXT("no_password_encryption"))
    {Running.bPasswordEncryption=Action==TEXT("password_encryption");return FString();}
    if(Action==TEXT("reload"))
    {bAwaitingReload=true;return IsStartupSaved()?TEXT("Proceed with reload? [confirm]"):TEXT("La configurazione non salvata verra' persa.\nProceed with reload? [confirm]");}
    if(Action==TEXT("show_mac"))
    {
        FString Out=TEXT("          Mac Address Table\nVlan    Mac Address       Type        Ports\n----    -----------       --------    -----\n");
        TArray<FString> Keys;LearnedMacTable.GetKeys(Keys);Keys.Sort();
        for(const FString& MAC:Keys)Out+=TEXT("--      ")+MAC+TEXT("    DYNAMIC     ")+LearnedMacTable.FindChecked(MAC)+TEXT("\n");
        return Out+TEXT("\nTabella appresa dal modello di rete locale; VLAN non specificata dalla sorgente.");
    }
    if(Action==TEXT("ping"))
    {
        uint32 Address=0;if(!ParseIPv4(Words.Last(),Address))return TEXT("% Indirizzo IPv4 non valido.");
        return ReachableTargets.Contains(Words.Last())?TEXT("!!!!!\nSuccess rate is 100 percent (5/5). Modello locale: nessun tempo misurato."):TEXT(".....\nSuccess rate is 0 percent (0/5). Esito del modello di rete locale.");
    }
    if (Action == TEXT("show_running")) return TEXT("Building configuration...\n\nCurrent configuration (modello locale):\n") + RenderConfiguration(Running);
    if (Action == TEXT("show_startup")) return TEXT("Startup configuration (copia locale salvata):\n") + RenderConfiguration(Startup);
    if (Action == TEXT("show_interfaces")) return ShowInterfaces();
    if (Action == TEXT("show_vlans")) return ShowVlans();
    if (Action == TEXT("copy"))
    {
        bAwaitingSaveConfirmation = true;
        return TEXT("Destination filename [startup-config]?");
    }
    if (Action == TEXT("save")) return SaveStartup();
    return TEXT("% Operazione non implementata nel sottoinsieme locale. Nessuna modifica applicata.");
}

FString FClaraSwitchCLI::SaveStartup()
{
    Startup = Running;
    bAwaitingSaveConfirmation = false;
    return TEXT("Building configuration...\n[OK]\nCopia startup salvata nel modello locale.");
}

FString FClaraSwitchCLI::RenderConfiguration(const FConfiguration& Config) const
{
    FString Result = TEXT("! Sottoinsieme didattico locale\nhostname ") + Config.Hostname + TEXT("\n!\n");
    TArray<int32> Vlans = Config.Vlans.Array();
    Vlans.Sort();
    for (int32 Id : Vlans) if (Id != 1) Result += FString::Printf(TEXT("vlan %d\n!\n"), Id);
    TArray<int32> Interfaces;
    Config.Interfaces.GetKeys(Interfaces);
    Interfaces.Sort();
    for (int32 Id : Interfaces)
    {
        const FInterface& Interface = Config.Interfaces.FindChecked(Id);
        Result += FString::Printf(TEXT("interface Vlan%d\n"), Id);
        Result += Interface.Address.IsEmpty() ? TEXT(" no ip address\n") : TEXT(" ip address ") + Interface.Address + TEXT(" ") + Interface.Mask + TEXT("\n");
        Result += Interface.bShutdown ? TEXT(" shutdown\n") : TEXT(" no shutdown\n");
        Result += TEXT("!\n");
    }
    if(!Config.EnableSecret.IsEmpty())Result+=TEXT("enable secret <impostato: hash Cisco non simulato>\n");
    if(!Config.DomainName.IsEmpty())Result+=TEXT("ip domain-name ")+Config.DomainName+TEXT("\n");
    if(!Config.Gateway.IsEmpty())Result+=TEXT("ip default-gateway ")+Config.Gateway+TEXT("\n");
    if(Config.SSHVersion)Result+=TEXT("ip ssh version ")+FString::FromInt(Config.SSHVersion)+TEXT("\n");
    if(Config.RSAKeyBits)Result+=TEXT("! RSA nel modello locale: ")+FString::FromInt(Config.RSAKeyBits)+TEXT(" bit; nessun materiale crittografico reale\n");
    if(Config.bPasswordEncryption)Result+=TEXT("service password-encryption\n! Type 7 e' offuscamento debole, non una protezione crittografica moderna; encoding Cisco non simulato\n");
    TArray<FString> Users;Config.Users.GetKeys(Users);Users.Sort();
    for(const FString& User:Users)Result+=TEXT("username ")+User+TEXT(" secret <impostato: hash Cisco non simulato>\n");
    if(!Config.Banner.IsEmpty())Result+=TEXT("banner motd #")+Config.Banner+TEXT("#\n");
    Result+=TEXT("line console 0\n");
    if(!Config.ConsolePassword.IsEmpty())Result+=TEXT(" password <impostata nel modello locale>\n");
    if(Config.bConsoleLogin)Result+=Config.bConsoleLoginLocal?TEXT(" login local\n"):TEXT(" login\n");
    Result+=TEXT("line vty 0 15\n");
    if(!Config.VTYPassword.IsEmpty())Result+=TEXT(" password <impostata nel modello locale>\n");
    if(Config.bVTYLogin)Result+=Config.bVTYLoginLocal?TEXT(" login local\n"):TEXT(" login\n");
    Result+=TEXT(" transport input ")+Config.Transport+TEXT("\n");
    return Result + TEXT("end");
}

FString FClaraSwitchCLI::ShowInterfaces() const
{
    FString Result = TEXT("Interface              IP-Address      OK? Method Status                Protocol\n");
    TArray<int32> Ids;
    Running.Interfaces.GetKeys(Ids);
    Ids.Sort();
    for (int32 Id : Ids)
    {
        const FInterface& Interface = Running.Interfaces.FindChecked(Id);
        Result += FString::Printf(TEXT("%-22s %-15s YES manual %-21s down\n"),
            *FString::Printf(TEXT("Vlan%d"), Id),
            Interface.Address.IsEmpty() ? TEXT("unassigned") : *Interface.Address,
            Interface.bShutdown ? TEXT("administratively down") : TEXT("up"));
    }
    return Result + TEXT("\nModello locale: Protocol non sale senza porte attive nella VLAN. Lo stato operativo e il traffico non sono modellati da questa CLI.");
}

FString FClaraSwitchCLI::ShowVlans() const
{
    FString Result = TEXT("VLAN Name                             Status    Ports\n---- -------------------------------- --------- ----------------\n");
    TArray<int32> Ids = Running.Vlans.Array();
    Ids.Sort();
    for (int32 Id : Ids)
        Result += FString::Printf(TEXT("%-4d %-32s active    --\n"), Id, Id == 1 ? TEXT("default") : *FString::Printf(TEXT("VLAN%04d"), Id));
    return Result + TEXT("\nAssegnazione delle porte non inclusa nel sottoinsieme locale.");
}

bool FClaraSwitchCLI::IsMissionConfigured() const
{
    const FInterface* Interface = Running.Interfaces.Find(10);
    return Running.Hostname.Equals(TEXT("AULA-01"), ESearchCase::IgnoreCase)
        && Running.Vlans.Contains(10) && Interface && !Interface->bShutdown
        && Interface->Address == TEXT("192.168.10.2") && Interface->Mask == TEXT("255.255.255.0")
        && ConfigurationIdentity(Running) == ConfigurationIdentity(Startup) && !bAwaitingSaveConfirmation;
}

FString FClaraSwitchCLI::GetMentorFeedback() const
{
    if (bAwaitingSaveConfirmation) return TEXT("Luca: la copia aspetta una conferma. Premi Invio per accettare startup-config; per ora il salvataggio non e' concluso.");
    if (!Running.Hostname.Equals(TEXT("AULA-01"), ESearchCase::IgnoreCase))
        return TEXT("Luca: il nome e' valido, ma il ticket identifica questo switch come AULA-01. L'hostname si cambia nel contesto (config)#; non ripara da solo la connettivita'. Parti da enable, poi configure terminal.");
    if (!Running.Vlans.Contains(10))
        return TEXT("Luca: il nome e' impostato. Ora crea la VLAN 10 dalla configurazione globale. Una SVI chiamata Vlan10 non crea automaticamente la VLAN nello switch.");
    const FInterface* Interface = Running.Interfaces.Find(10);
    if (!Interface) return TEXT("Luca: la VLAN esiste. Seleziona la sua interfaccia di gestione con interface vlan 10 dalla configurazione globale. Guarda come cambia il prompt.");
    if (Interface->Address != TEXT("192.168.10.2") || Interface->Mask != TEXT("255.255.255.0"))
        return TEXT("Luca: il ticket assegna alla SVI 192.168.10.2 con mask 255.255.255.0. Usa ip address nel contesto (config-if)#. Un altro indirizzo valido non e' un errore di sintassi: e' una configurazione diversa dal ticket.");
    if (Interface->bShutdown)
        return TEXT("Luca: l'indirizzo e' corretto, ma la SVI e' amministrativamente disabilitata. Cerca l'impostazione shutdown e la sua negazione nell'aiuto del contesto interfaccia.");
    if (ConfigurationIdentity(Running) != ConfigurationIdentity(Startup))
        return TEXT("Luca: la configurazione in uso e' pronta, ma la copia startup e' diversa. Torna a # con end, confronta show running-config e show startup-config, poi salva con copy running-config startup-config o write memory.");
    return TEXT("Luca: nome, VLAN, SVI, indirizzo e salvataggio sono coerenti con il ticket. Hai preparato la configurazione. La raggiungibilita' richiede anche porte e rete operative: questa CLI locale non esegue quella verifica.");
}

FString FClaraSwitchCLI::GetManagementAddress() const
{
    const FInterface* Preferred=Running.Interfaces.Find(10);
    if(Preferred&&!Preferred->Address.IsEmpty())return Preferred->Address;
    TArray<int32> Ids;Running.Interfaces.GetKeys(Ids);Ids.Sort();
    for(int32 Id:Ids)if(!Running.Interfaces.FindChecked(Id).Address.IsEmpty())return Running.Interfaces.FindChecked(Id).Address;
    return FString();
}
bool FClaraSwitchCLI::IsSVIUp() const
{
    const FString Address=GetManagementAddress();
    for(const auto& Entry:Running.Interfaces)
        if(!Address.IsEmpty()&&Entry.Value.Address==Address&&!Entry.Value.bShutdown&&Running.Vlans.Contains(Entry.Key))return true;
    return false;
}
bool FClaraSwitchCLI::IsStartupSaved() const{return ConfigurationIdentity(Running)==ConfigurationIdentity(Startup)&&!bAwaitingSaveConfirmation;}
bool FClaraSwitchCLI::IsSSHConfigured() const
{return !Running.DomainName.IsEmpty()&&Running.RSAKeyBits>=2048&&Running.SSHVersion==2&&!Running.Users.IsEmpty()&&Running.bVTYLoginLocal&&Running.Transport==TEXT("ssh");}
FString FClaraSwitchCLI::ValidateDay2(const FString& ExpectedName,const FString& ExpectedIP,bool IncludeSaved) const
{
    if(!Running.Hostname.Equals(ExpectedName,ESearchCase::IgnoreCase))return TEXT("Il nome dello switch non corrisponde al ticket.");
    if(GetManagementAddress()!=ExpectedIP)return TEXT("L'indirizzo di gestione non corrisponde al ticket.");
    if(!IsSVIUp())return TEXT("La SVI deve appartenere a una VLAN esistente ed essere abilitata amministrativamente.");
    if(Running.Gateway.IsEmpty())return TEXT("Manca il gateway predefinito di gestione.");
    if(Running.EnableSecret.IsEmpty())return TEXT("Manca enable secret per proteggere l'accesso privilegiato.");
    if(Running.ConsolePassword.IsEmpty()||!Running.bConsoleLogin||Running.bConsoleLoginLocal)return TEXT("La console richiede password e login per questo incarico.");
    if(!IsSSHConfigured())return TEXT("Verifica dominio, RSA almeno 2048 bit, SSH v2, utente locale e VTY con login local e solo SSH.");
    if(!Running.bPasswordEncryption)return TEXT("Attiva service password-encryption, ricordando che Type 7 e' solo offuscamento debole.");
    if(Running.Banner.IsEmpty())return TEXT("Manca il messaggio di accesso MOTD richiesto.");
    if(IncludeSaved&&!IsStartupSaved())return TEXT("Le modifiche non sono ancora salvate nella startup-config.");
    return FString();
}
void FClaraSwitchCLI::ReloadFromStartup()
{
    Running=Startup;Mode=EMode::UserExec;CurrentInterface=1;
    bAwaitingSaveConfirmation=false;bAwaitingRSA=false;bAwaitingBanner=false;bAwaitingReload=false;
    BannerDelimiter=0;PendingBanner.Empty();LearnedMacTable.Empty();
}
FString FClaraSwitchCLI::GenerateRSA(int32 Bits)
{
    if(Bits!=512&&Bits!=768&&Bits!=1024&&Bits!=2048&&Bits!=3072&&Bits!=4096)return TEXT("% Dimensione supportata: 512, 768, 1024, 2048, 3072 o 4096 bit.");
    Running.RSAKeyBits=Bits;bAwaitingRSA=false;
    return TEXT("Identita' RSA impostata nel modello locale: ")+FString::FromInt(Bits)+TEXT(" bit. Nessuna chiave reale generata.");
}
FString FClaraSwitchCLI::ConfigurationIdentity(const FConfiguration& Config) const
{
    FString Identity=RenderConfiguration(Config)+TEXT("\n__local_credentials__\n")+Config.EnableSecret+TEXT("\n")+Config.ConsolePassword+TEXT("\n")+Config.VTYPassword;
    TArray<FString> Users;Config.Users.GetKeys(Users);Users.Sort();
    for(const FString& User:Users)Identity+=TEXT("\n")+User+TEXT("=")+Config.Users.FindChecked(User);
    return Identity;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
void ConfigureClaraSwitch(FClaraSwitchCLI& CLI)
{
    for (const TCHAR* Command : {
        TEXT("en"), TEXT("conf t"), TEXT("hostname AULA-01"), TEXT("vlan 10"), TEXT("exit"),
        TEXT("int vlan10"), TEXT("ip address 192.168.10.2 255.255.255.0"), TEXT("no sh"), TEXT("end")})
        CLI.Execute(Command);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraSwitchModesTest, "Clara.SwitchCLI.ModesAndAbbreviations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FClaraSwitchModesTest::RunTest(const FString& Parameters)
{
    FClaraSwitchCLI CLI;
    TestEqual(TEXT("Initial user prompt"), CLI.GetPrompt(), FString(TEXT("Switch>")));
    TestTrue(TEXT("Configuration cannot execute in user mode"), CLI.Execute(TEXT("hostname AULA-01")).Contains(TEXT("Invalid input")));
    CLI.Execute(TEXT("en"));
    TestEqual(TEXT("Abbreviated enable"), CLI.GetPrompt(), FString(TEXT("Switch#")));
    TestTrue(TEXT("Ambiguous configure/copy rejected"), CLI.Execute(TEXT("c")).Contains(TEXT("Ambiguous")));
    TestEqual(TEXT("Ambiguity does not change mode"), CLI.GetPrompt(), FString(TEXT("Switch#")));
    TestTrue(TEXT("Missing configure target is incomplete"), CLI.Execute(TEXT("configure")).Contains(TEXT("Incomplete")));
    CLI.Execute(TEXT("conf t"));
    CLI.Execute(TEXT("host FRONT-DESK"));
    TestEqual(TEXT("Wrong mission hostname is valid configuration"), CLI.GetPrompt(), FString(TEXT("FRONT-DESK(config)#")));
    TestFalse(TEXT("Valid different name does not satisfy mission"), CLI.IsMissionConfigured());
    TestTrue(TEXT("Tutor explains required name"), CLI.GetMentorFeedback().Contains(TEXT("AULA-01")));
    CLI.Execute(TEXT("vlan 20"));
    TestTrue(TEXT("VLAN submode"), CLI.GetPrompt().Contains(TEXT("(config-vlan)#")));
    CLI.Execute(TEXT("exit"));
    CLI.Execute(TEXT("int vl10"));
    TestTrue(TEXT("Compact abbreviated SVI name"), CLI.GetPrompt().Contains(TEXT("(config-if)#")));
    TestTrue(TEXT("Unknown command returns caret"), CLI.Execute(TEXT("router ospf 1")).Contains(TEXT("^")));
    CLI.Execute(TEXT("end"));
    TestTrue(TEXT("Abbreviated show chain"), CLI.Execute(TEXT("sh ip int br")).Contains(TEXT("Vlan10")));
    CLI.Execute(TEXT("disable"));
    TestEqual(TEXT("Disable returns to user mode"), CLI.GetPrompt(), FString(TEXT("FRONT-DESK>")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraSwitchHelpTest, "Clara.SwitchCLI.ContextHelpAndTab", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FClaraSwitchHelpTest::RunTest(const FString& Parameters)
{
    FClaraSwitchCLI CLI;
    TestEqual(TEXT("Unique root completion"), CLI.Complete(TEXT("en")), FString(TEXT("enable ")));
    TestTrue(TEXT("Empty-line help lists available root commands"), CLI.Help(TEXT("")).Contains(TEXT("enable")));
    TestTrue(TEXT("In-word help returns keyword"), CLI.Help(TEXT("sh")).Contains(TEXT("show")));
    TestFalse(TEXT("In-word help does not jump to next word"), CLI.Help(TEXT("sh")).Contains(TEXT("running-config")));
    TestTrue(TEXT("Whitespace help advances context"), CLI.Help(TEXT("show ")).Contains(TEXT("ip")));
    TestTrue(TEXT("Question mark after whitespace preserves next-word context"), CLI.Execute(TEXT("show ?")).Contains(TEXT("ip")));
    CLI.Execute(TEXT("enable"));
    TestEqual(TEXT("Ambiguous words share only common prefix"), CLI.Complete(TEXT("c")), FString(TEXT("co")));
    TestEqual(TEXT("Contextual multiword completion"), CLI.Complete(TEXT("conf t")), FString(TEXT("configure terminal ")));
    TestTrue(TEXT("In-word interface keyword help"), CLI.Help(TEXT("show ip int")).Contains(TEXT("interface")));
    TestFalse(TEXT("Word help does not skip interface"), CLI.Help(TEXT("show ip int")).Contains(TEXT("brief")));
    TestTrue(TEXT("Next-word help provides brief"), CLI.Help(TEXT("show ip interface ")).Contains(TEXT("brief")));
    TestTrue(TEXT("Complete command offers enter"), CLI.Help(TEXT("show ip interface brief ")).Contains(TEXT("<cr>")));
    TestEqual(TEXT("Tab does not invent parameter values"), CLI.Complete(TEXT("configure terminal unexpected")), FString(TEXT("configure terminal unexpected")));
    const FString Before = CLI.GetPrompt();
    CLI.Execute(TEXT("conf ?"));
    TestEqual(TEXT("Question mark does not execute configuration"), CLI.GetPrompt(), Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraSwitchValidationTest, "Clara.SwitchCLI.InvalidValuesNeverMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FClaraSwitchValidationTest::RunTest(const FString& Parameters)
{
    FClaraSwitchCLI CLI;
    ConfigureClaraSwitch(CLI);
    CLI.Execute(TEXT("write mem"));
    TestTrue(TEXT("Known correct configuration can complete"), CLI.IsMissionConfigured());
    CLI.Execute(TEXT("conf t"));
    CLI.Execute(TEXT("int vlan 10"));
    TestTrue(TEXT("Out-of-range address rejected"), CLI.Execute(TEXT("ip address 192.168.10.256 255.255.255.0")).Contains(TEXT("non valido")));
    TestTrue(TEXT("Noncontiguous mask rejected"), CLI.Execute(TEXT("ip address 192.168.10.2 255.0.255.0")).Contains(TEXT("Bad mask")));
    TestTrue(TEXT("Network address rejected"), CLI.Execute(TEXT("ip address 192.168.10.0 255.255.255.0")).Contains(TEXT("Bad mask")));
    TestTrue(TEXT("Broadcast address rejected"), CLI.Execute(TEXT("ip address 192.168.10.255 255.255.255.0")).Contains(TEXT("Bad mask")));
    TestTrue(TEXT("Multicast address rejected"), CLI.Execute(TEXT("ip address 224.1.1.1 255.255.255.0")).Contains(TEXT("non valido")));
    TestTrue(TEXT("Rejected commands preserve saved correct state"), CLI.IsMissionConfigured());
    CLI.Execute(TEXT("ip address 192.168.10.20 255.255.255.0"));
    TestFalse(TEXT("Valid wrong address remains wrong for mission"), CLI.IsMissionConfigured());
    CLI.Execute(TEXT("shutdown"));
    CLI.Execute(TEXT("end"));
    TestTrue(TEXT("Administrative state is shown separately"), CLI.Execute(TEXT("sh ip int br")).Contains(TEXT("administratively down")));
    TestTrue(TEXT("Startup remains original snapshot"), CLI.Execute(TEXT("show start")).Contains(TEXT("ip address 192.168.10.2 255.255.255.0")));
    TestTrue(TEXT("Running preserves valid user mistake"), CLI.Execute(TEXT("show run")).Contains(TEXT("ip address 192.168.10.20 255.255.255.0")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraSwitchSnapshotTest, "Clara.SwitchCLI.StartupConfirmationAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FClaraSwitchSnapshotTest::RunTest(const FString& Parameters)
{
    FClaraSwitchCLI CLI;
    ConfigureClaraSwitch(CLI);
    TestFalse(TEXT("Unsaved correct config is not complete"), CLI.IsMissionConfigured());
    CLI.Execute(TEXT("copy run start"));
    TestTrue(TEXT("Copy requests destination confirmation"), CLI.IsAwaitingSaveConfirmation());
    TestTrue(TEXT("Prompt shows filename request"), CLI.GetPrompt().Contains(TEXT("Destination filename")));
    TestFalse(TEXT("Pending copy is not saved"), CLI.IsMissionConfigured());
    CLI.Execute(TEXT("other-file"));
    TestTrue(TEXT("Unsupported destination never silently saves"), CLI.IsAwaitingSaveConfirmation());
    CLI.Execute(TEXT(""));
    TestFalse(TEXT("Enter closes save confirmation"), CLI.IsAwaitingSaveConfirmation());
    TestTrue(TEXT("Confirmed snapshot satisfies mission"), CLI.IsMissionConfigured());
    CLI.Execute(TEXT("conf t"));
    CLI.Execute(TEXT("hostname WRONG-NAME"));
    CLI.Execute(TEXT("end"));
    TestTrue(TEXT("Startup does not mutate with running name"), CLI.Execute(TEXT("show startup-config")).Contains(TEXT("hostname AULA-01")));
    TestFalse(TEXT("Changed configuration requires correction and resave"), CLI.IsMissionConfigured());
    CLI.Reset();
    TestEqual(TEXT("Tutor reset returns to initial prompt"), CLI.GetPrompt(), FString(TEXT("Switch>")));
    TestFalse(TEXT("Tutor reset clears completion"), CLI.IsMissionConfigured());
    TestFalse(TEXT("Tutor reset clears pending copy"), CLI.IsAwaitingSaveConfirmation());
    CLI.Execute(TEXT("en"));
    const FString Startup = CLI.Execute(TEXT("show start"));
    TestTrue(TEXT("Reset also resets startup"), Startup.Contains(TEXT("hostname Switch")));
    TestFalse(TEXT("Reset removes learned VLAN config"), Startup.Contains(TEXT("vlan 10")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraSwitchDay2Test, "Clara.SwitchCLI.DayTwoSecurityAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FClaraSwitchDay2Test::RunTest(const FString& Parameters)
{
    FClaraSwitchCLI CLI;
    for(const TCHAR* C:{TEXT("enable"),TEXT("conf t"),TEXT("hostname AULA-02"),TEXT("vlan 10"),TEXT("exit"),TEXT("int vlan10"),TEXT("ip address 192.168.10.3 255.255.255.0"),TEXT("no shut"),TEXT("exit"),TEXT("enable secret Privato1"),TEXT("line console 0"),TEXT("password Console1"),TEXT("login"),TEXT("exit"),TEXT("ip domain-name clara.local")})CLI.Execute(C);
    CLI.Execute(TEXT("crypto key generate rsa"));
    TestTrue(TEXT("RSA interactive entry exposed"),CLI.IsAwaitingInput());CLI.Execute(TEXT("2048"));
    for(const TCHAR* C:{TEXT("username tecnico secret Personale1"),TEXT("ip ssh version 2"),TEXT("line vty 0 15"),TEXT("login local"),TEXT("transport input ssh"),TEXT("exit"),TEXT("banner motd #Accesso autorizzato#"),TEXT("service password-encryption"),TEXT("ip default-gateway 192.168.10.1")})CLI.Execute(C);
    TestTrue(TEXT("do show works without leaving global configuration"),CLI.Execute(TEXT("do sh run")).Contains(TEXT("hostname AULA-02")));
    TestTrue(TEXT("SSH requires actual modeled prerequisites"),CLI.IsSSHConfigured());
    TestFalse(TEXT("Day 2 cannot complete before save"),CLI.ValidateDay2(TEXT("AULA-02"),TEXT("192.168.10.3")).IsEmpty());
    TestTrue(TEXT("Pre-reboot challenge can inspect correct unsaved running state"),CLI.ValidateDay2(TEXT("AULA-02"),TEXT("192.168.10.3"),false).IsEmpty());
    CLI.Execute(TEXT("end"));CLI.Execute(TEXT("write memory"));
    TestTrue(TEXT("Complete security state validates"),CLI.ValidateDay2(TEXT("AULA-02"),TEXT("192.168.10.3")).IsEmpty());
    CLI.Execute(TEXT("conf t"));CLI.Execute(TEXT("enable secret Changed1"));
    TestFalse(TEXT("Redacted secrets still affect startup snapshot equality"),CLI.IsStartupSaved());
    CLI.Execute(TEXT("end"));CLI.Execute(TEXT("reload"));TestTrue(TEXT("Reload requires confirmation"),CLI.IsAwaitingInput());CLI.Execute(TEXT(""));
    TestEqual(TEXT("Reload restores saved identity"),CLI.GetHostname(),FString(TEXT("AULA-02")));
    TestTrue(TEXT("Reload restores saved secret and configuration"),CLI.IsStartupSaved());
    TMap<FString,FString> MAC;MAC.Add(TEXT("0011.2233.4455"),TEXT("Gi1/0/1"));CLI.SetLearnedMacTable(MAC);
    TestTrue(TEXT("MAC table derives from injected learned state"),CLI.Execute(TEXT("show mac address-table")).Contains(TEXT("0011.2233.4455")));
    TSet<FString> Targets;Targets.Add(TEXT("192.168.10.20"));CLI.SetReachableTargets(Targets);
    TestTrue(TEXT("IOS ping uses reachable model targets"),CLI.Execute(TEXT("ping 192.168.10.20")).Contains(TEXT("100 percent")));
    TestTrue(TEXT("Unknown destination does not succeed"),CLI.Execute(TEXT("ping 192.168.10.99")).Contains(TEXT("Success rate is 0 percent")));
    return true;
}
#endif
