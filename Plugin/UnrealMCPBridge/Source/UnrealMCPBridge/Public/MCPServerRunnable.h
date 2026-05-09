#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "Sockets.h"
#include "MCPCommandRegistry.h"

// Worker thread that accepts TCP connections and processes JSON-RPC 2.0 requests.
// Requests are dispatched to the GameThread; responses are sent back on this thread.
class UNREALMCPBRIDGE_API FMCPServerRunnable : public FRunnable
{
public:
    FMCPServerRunnable(
        const FString& Host,
        int32 Port,
        int32 MaxLineBytes,
        int32 DispatchTimeoutSeconds,
        TSharedRef<FMCPCommandRegistry> Registry);

    virtual ~FMCPServerRunnable();

    // FRunnable interface.
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    // Spawn the worker thread and begin accepting connections.
    void StartThread();

    // Signal stop, block until the thread finishes, then release all resources.
    // Never calls Kill — always waits for clean completion.
    void RequestStopAndWait();

private:
    // Read bytes from ClientSocket into a line buffer until '\n' is found.
    // Returns false when the connection is closed or the line exceeds MaxLineBytes.
    bool ReceiveLine(FSocket* ClientSocket, FString& OutLine, bool& bOutExceededLimit);

    // Send a serialized response followed by '\n' to ClientSocket.
    bool SendResponse(FSocket* ClientSocket, const FString& Serialized);

    // Dispatch a parsed request to the GameThread and wait for the result.
    MCPProtocol::FMCPResponse DispatchToGameThread(const MCPProtocol::FMCPRequest& Request);

    const FString BindHost;
    const int32 BindPort;
    const int32 MaxLineBytesLimit;
    const int32 DispatchTimeoutSec;
    TSharedRef<FMCPCommandRegistry> CommandRegistry;

    FSocket* ListenSocket = nullptr;
    FRunnableThread* Thread = nullptr;
    FThreadSafeBool bStopRequested;
};
