#include "MCPProtocol.h"
#include "UnrealMCPBridgeModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace MCPProtocol
{

bool FMCPRequest::IsNotification() const
{
    // A request with no id or with a JSON null id is a notification.
    return !Id.IsValid() || Id->Type == EJson::Null;
}

bool ParseRequest(const FString& Line, FMCPRequest& OutRequest, FMCPError& OutError)
{
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutError.Code    = FMCPError::ParseError;
        OutError.Message = TEXT("Parse error: invalid JSON");
        return false;
    }

    // Validate jsonrpc field.
    FString JsonrpcVersion;
    if (!RootObject->TryGetStringField(TEXT("jsonrpc"), JsonrpcVersion) || JsonrpcVersion != TEXT("2.0"))
    {
        OutError.Code    = FMCPError::InvalidRequest;
        OutError.Message = TEXT("Invalid Request: missing or wrong jsonrpc version");
        return false;
    }
    OutRequest.Jsonrpc = JsonrpcVersion;

    // Preserve id as-is (string, number, null, or absent).
    const TSharedPtr<FJsonValue>* IdField = RootObject->Values.Find(TEXT("id"));
    OutRequest.Id = IdField ? *IdField : nullptr;

    // Validate method field.
    FString Method;
    if (!RootObject->TryGetStringField(TEXT("method"), Method) || Method.IsEmpty())
    {
        OutError.Code    = FMCPError::InvalidRequest;
        OutError.Message = TEXT("Invalid Request: missing or empty method");
        return false;
    }
    OutRequest.Method = Method;

    // Params is optional; if present it must be an object.
    const TSharedPtr<FJsonValue>* ParamsValue = RootObject->Values.Find(TEXT("params"));
    if (ParamsValue && ParamsValue->IsValid())
    {
        if ((*ParamsValue)->Type == EJson::Object)
        {
            OutRequest.Params = (*ParamsValue)->AsObject();
        }
        else if ((*ParamsValue)->Type != EJson::Null)
        {
            OutError.Code    = FMCPError::InvalidParams;
            OutError.Message = TEXT("Invalid params: must be an object");
            return false;
        }
    }

    return true;
}

FString SerializeResponse(const FMCPResponse& Response)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("jsonrpc"), Response.Jsonrpc);

    // Id field — preserve null when absent.
    if (Response.Id.IsValid())
    {
        Root->SetField(TEXT("id"), Response.Id);
    }
    else
    {
        Root->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
    }

    // Error takes precedence over result when both are present.
    if (Response.Error.IsSet())
    {
        const FMCPError& Err = Response.Error.GetValue();
        TSharedRef<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
        ErrorObject->SetNumberField(TEXT("code"), static_cast<double>(Err.Code));
        ErrorObject->SetStringField(TEXT("message"), Err.Message);
        if (Err.Data.IsValid())
        {
            ErrorObject->SetField(TEXT("data"), Err.Data);
        }
        Root->SetObjectField(TEXT("error"), ErrorObject);
    }
    else if (Response.Result.IsValid())
    {
        Root->SetObjectField(TEXT("result"), Response.Result);
    }
    else
    {
        // Neither result nor error — emit null result to stay spec-compliant.
        Root->SetField(TEXT("result"), MakeShared<FJsonValueNull>());
    }

    FString Output;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}

FMCPResponse MakeError(TSharedPtr<FJsonValue> Id, int32 Code, const FString& Message)
{
    FMCPResponse Response;
    Response.Id = Id;
    FMCPError Err;
    Err.Code    = Code;
    Err.Message = Message;
    Response.Error = Err;
    return Response;
}

FMCPResponse MakeResult(TSharedPtr<FJsonValue> Id, TSharedPtr<FJsonObject> Result)
{
    FMCPResponse Response;
    Response.Id     = Id;
    Response.Result = Result;
    return Response;
}

} // namespace MCPProtocol
