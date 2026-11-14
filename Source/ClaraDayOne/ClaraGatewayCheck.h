#pragma once
class UWorld;

/** Explicit opt-in integration check. Reads a live IOS console; no patch, config, AI or deployment commands. */
void RunClaraGatewayCheck(UWorld* World);

class AClaraStoryDirector;
void RunClaraGatewayPortCheck(UWorld* World,AClaraStoryDirector* Story);
