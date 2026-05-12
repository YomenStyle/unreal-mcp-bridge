#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MCPGraphEditLibrary.generated.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

UCLASS()
class UNREALMCPBRIDGE_API UMCPGraphEditLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Adds a K2Node_CallFunction node to the named graph that calls
    // FunctionClassPath::FunctionName. Returns the new node's name, or empty
    // string on failure.
    UFUNCTION(BlueprintCallable, Category = "MCP|Graph", meta = (DevelopmentOnly))
    static FString AddCallFunctionNode(
        UBlueprint* Blueprint,
        const FString& GraphName,
        const FString& FunctionClassPath,
        const FString& FunctionName,
        int32 PosX,
        int32 PosY);

    // Adds a K2Node_IfThenElse (Branch) node. Returns the new node's name,
    // or empty string on failure.
    UFUNCTION(BlueprintCallable, Category = "MCP|Graph", meta = (DevelopmentOnly))
    static FString AddIfThenElseNode(
        UBlueprint* Blueprint,
        const FString& GraphName,
        int32 PosX,
        int32 PosY);

    // Connects two pins. Returns true on success.
    UFUNCTION(BlueprintCallable, Category = "MCP|Graph", meta = (DevelopmentOnly))
    static bool ConnectPins(
        UBlueprint* Blueprint,
        const FString& GraphName,
        const FString& SrcNodeName,
        const FString& SrcPinName,
        const FString& DstNodeName,
        const FString& DstPinName);

    // Disconnects a specific link between two pins. Returns true on success.
    UFUNCTION(BlueprintCallable, Category = "MCP|Graph", meta = (DevelopmentOnly))
    static bool DisconnectPinLink(
        UBlueprint* Blueprint,
        const FString& GraphName,
        const FString& NodeA,
        const FString& PinA,
        const FString& NodeB,
        const FString& PinB);

    // Compiles the Blueprint and marks the package dirty.
    UFUNCTION(BlueprintCallable, Category = "MCP|Graph", meta = (DevelopmentOnly))
    static bool CompileBlueprint(UBlueprint* Blueprint);

    // Removes a node from the named graph. Returns true on success.
    UFUNCTION(BlueprintCallable, Category = "MCP|Graph", meta = (DevelopmentOnly))
    static bool RemoveNode(
        UBlueprint* Blueprint,
        const FString& GraphName,
        const FString& NodeName);

private:
    static UEdGraph* FindGraphByName(UBlueprint* BP, const FString& GraphName);
    static UEdGraphNode* FindNodeByName(UEdGraph* Graph, const FString& NodeName);
    static UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName);
};
