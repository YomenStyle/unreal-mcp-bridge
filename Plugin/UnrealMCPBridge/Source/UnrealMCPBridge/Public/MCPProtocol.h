#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace MCPProtocol
{

// Standard JSON-RPC 2.0 error codes and server-defined range.
struct FMCPError
{
    static constexpr int32 ParseError      = -32700;
    static constexpr int32 InvalidRequest  = -32600;
    static constexpr int32 MethodNotFound  = -32601;
    static constexpr int32 InvalidParams   = -32602;
    static constexpr int32 InternalError   = -32603;
    static constexpr int32 ServerErrorMin  = -32099;
    static constexpr int32 ServerErrorMax  = -32000;

    int32 Code = 0;
    FString Message;
    TSharedPtr<FJsonValue> Data; // optional extra diagnostic payload
};

// Parsed inbound JSON-RPC 2.0 request.
struct FMCPRequest
{
    FString Jsonrpc; // expected "2.0"
    TSharedPtr<FJsonValue> Id; // string | number | null — null/missing means notification
    FString Method;
    TSharedPtr<FJsonObject> Params;

    // True when the request has no id (or id is JSON null) — no response should be sent.
    bool IsNotification() const;
};

// Outbound JSON-RPC 2.0 response.
struct FMCPResponse
{
    FString Jsonrpc = TEXT("2.0");
    TSharedPtr<FJsonValue> Id;
    TSharedPtr<FJsonObject> Result; // populated on success
    TOptional<FMCPError> Error;     // populated on failure; takes precedence over Result
};

// Parse one newline-delimited UTF-8 line into a request.
// Returns false and populates OutError when parsing fails.
bool ParseRequest(const FString& Line, FMCPRequest& OutRequest, FMCPError& OutError);

// Serialize a response to a UTF-8 JSON string (without trailing newline).
FString SerializeResponse(const FMCPResponse& Response);

// Convenience constructors.
FMCPResponse MakeError(TSharedPtr<FJsonValue> Id, int32 Code, const FString& Message);
FMCPResponse MakeResult(TSharedPtr<FJsonValue> Id, TSharedPtr<FJsonObject> Result);

} // namespace MCPProtocol
