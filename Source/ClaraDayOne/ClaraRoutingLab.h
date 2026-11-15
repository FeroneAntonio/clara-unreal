#pragma once

#include "CoreMinimal.h"
#include "ClaraSwitchCLI.h"
class FJsonObject;

/** Offline, five-device routing lesson. No command reaches a host OS or network. */
class CLARADAYONE_API FClaraRoutingLab
{
public:
    FClaraRoutingLab();
    void Reset();
    FString Execute(const FString& Device, const FString& Input);
    FString GetPrompt(const FString& Device) const;
    bool IsAwaitingInput(const FString& Device) const;
    bool IsReadingBanner(const FString& Device) const;
    void SetCable(const FString& A, const FString& B, bool Connected);
    bool HasCable(const FString& A, const FString& B) const;
    FString Ping(const FString& Source, const FString& Target) const;
    FString Traceroute(const FString& Source, const FString& Target) const;
    bool IsDeviceConfigured(const FString& Device, bool IncludeSaved=true) const;
    bool IsBaseConfigured(bool IncludeSaved=false) const;
    bool HasRoute(const FString& Device, const FString& Network, const FString& Mask, const FString& NextHop) const;
    bool IsStartupSaved(const FString& Device) const;
    TArray<FString> Validate(bool IncludeSaved=true) const;
    void InjectFaults();
    void Reboot();
    TSharedRef<FJsonObject> ExportState() const;
    bool ImportState(const TSharedPtr<FJsonObject>& State);

private:
    enum class EMode : uint8 { User, Privileged, Config, Interface, Console };
    struct FInterface { FString Address, Mask; bool Up=false; };
    struct FRoute { FString Network, Mask, NextHop; };
    struct FConfig
    {
        FString Hostname=TEXT("Router"), Secret, ConsolePassword;
        bool ConsoleLogin=false;
        FInterface Interfaces[2];
        TArray<FRoute> Routes;
    };
    struct FRouter
    {
        FConfig Running, Startup;
        EMode Mode=EMode::User;
        int32 Interface=0;
        bool AwaitingSave=false, AwaitingReload=false;
    };
    FRouter R1, R2;
    FClaraSwitchCLI SW2;
    TSet<FString> Cables;
    static FString Edge(const FString& A, const FString& B);
    const FRouter* Router(const FString& Id) const;
    FRouter* Router(const FString& Id);
    FString ExecuteRouter(FRouter& Router, const FString& Id, const FString& Input);
    FString RenderConfig(const FConfig& Config) const;
    FString ShowRoutes(const FString& Id) const;
    FString ShowInterfaces(const FString& Id) const;
    bool InterfaceActive(const FString& Id, int32 Index) const;
    bool FindRoute(const FString& Id, const FString& Target, int32& Interface, FString& NextHop) const;
    FString SourceAddress(const FString& Device, const FString& Target) const;
    FString AddressOwner(const FString& Address) const;
    bool Path(const FString& Source, const FString& Target, TArray<FString>& Hops) const;
    static TSharedRef<FJsonObject> WriteConfig(const FConfig& Config);
    static bool ReadConfig(const TSharedPtr<FJsonObject>& Object, FConfig& Config);
};
