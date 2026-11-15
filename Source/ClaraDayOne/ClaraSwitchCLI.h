#pragma once

#include "CoreMinimal.h"
class FJsonObject;

/** Stateful, deliberately limited IOS-style lesson model. It does not execute Cisco IOS. */
class CLARADAYONE_API FClaraSwitchCLI
{
public:
    FClaraSwitchCLI();
    FString Execute(const FString& Input);
    FString GetPrompt() const;
    FString Complete(const FString& Prefix) const;
    FString Help(const FString& Prefix) const;
    bool IsMissionConfigured() const;
    bool IsAwaitingSaveConfirmation() const { return bAwaitingSaveConfirmation; }
    void Reset();
    FString GetMentorFeedback() const;
    FString GetHostname() const { return Running.Hostname; }
    FString GetManagementAddress() const;
    FString GetGateway() const { return Running.Gateway; }
    bool IsSVIUp() const;
    bool IsStartupSaved() const;
    bool IsSSHConfigured() const;
    bool AuthenticateLocalSSH(const FString& User, const FString& Password) const;
    FString GetRunningConfigurationFingerprint() const;
    bool IsAwaitingInput() const { return bAwaitingSaveConfirmation || bAwaitingRSA || bAwaitingBanner || bAwaitingReload; }
    bool IsReadingBanner() const { return bAwaitingBanner; }
    FString ValidateDay2(const FString& ExpectedName, const FString& ExpectedIP, bool IncludeSaved=true) const;
    void ReloadFromStartup();
    void SetLearnedMacTable(const TMap<FString,FString>& Table) { LearnedMacTable=Table; }
    void SetReachableTargets(const TSet<FString>& Targets) { ReachableTargets=Targets; }
    TSharedRef<FJsonObject> ExportState() const;
    bool ImportState(const TSharedPtr<FJsonObject>& State);

private:
    enum class EMode : uint8 { UserExec, PrivilegedExec, GlobalConfig, VlanConfig, InterfaceConfig, LineConsole, LineVTY };
    struct FInterface
    {
        FString Address;
        FString Mask;
        bool bShutdown = true;
    };
    struct FConfiguration
    {
        FString Hostname = TEXT("Switch");
        TSet<int32> Vlans;
        TMap<int32, FInterface> Interfaces;
        FString EnableSecret, ConsolePassword, VTYPassword, DomainName, Gateway, Banner;
        TMap<FString,FString> Users;
        bool bConsoleLogin=false, bConsoleLoginLocal=false, bVTYLogin=false, bVTYLoginLocal=false, bPasswordEncryption=false;
        FString Transport=TEXT("all");
        int32 RSAKeyBits=0, SSHVersion=0;
    };
    EMode Mode = EMode::UserExec;
    FConfiguration Running;
    FConfiguration Startup;
    int32 CurrentInterface = 1;
    bool bAwaitingSaveConfirmation = false;
    bool bAwaitingRSA=false, bAwaitingBanner=false, bAwaitingReload=false;
    TCHAR BannerDelimiter=0;
    FString PendingBanner;
    TMap<FString,FString> LearnedMacTable;
    TSet<FString> ReachableTargets;

    struct FRule
    {
        FString Pattern;
        FString Action;
    };
    struct FResolution
    {
        TArray<FRule> Rules;
        TArray<FString> Canonical;
        FString Error;
    };
    TArray<FRule> GetRules() const;
    FResolution Resolve(const TArray<FString>& Words, const FString& Original) const;
    FString RenderConfiguration(const FConfiguration& Config) const;
    FString ShowInterfaces() const;
    FString ShowVlans() const;
    FString SaveStartup();
    FString ConfigurationIdentity(const FConfiguration& Config) const;
    FString GenerateRSA(int32 Bits);
    FString ApplyRule(const FRule& Rule, const TArray<FString>& Words);
    TArray<FString> GetSuggestions(const FString& Prefix, TArray<FString>& Canonical, bool& bTrailingSpace, FString& Partial) const;
};
