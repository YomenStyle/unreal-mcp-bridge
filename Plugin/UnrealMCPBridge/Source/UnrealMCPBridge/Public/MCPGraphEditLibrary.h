#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MCPGraphEditLibrary.generated.h"

class UBlueprint;
class UAnimBlueprint;
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

    // Adds a K2Node_VariableGet (self-context) for PropertyName, a property on the Blueprint's
    // own generated/parent class. Works in AnimGraph nested graphs too (GraphName is resolved
    // the same way as the other Add*Node functions). Returns the new node's name, or empty on failure.
    UFUNCTION(BlueprintCallable, Category = "MCP|Graph", meta = (DevelopmentOnly))
    static FString AddVariableGetNode(
        UBlueprint* Blueprint,
        const FString& GraphName,
        const FString& PropertyName,
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

    // Finds and removes a UEdGraph subobject of Blueprint by name that isn't reachable from any
    // live node (e.g. an inner state-machine graph orphaned by a node deleted before this class's
    // RemoveNode fix started calling DestroyNode()). Returns true if a matching orphan was found
    // and removed.
    UFUNCTION(BlueprintCallable, Category = "MCP|Graph", meta = (DevelopmentOnly))
    static bool RemoveOrphanedGraph(
        UBlueprint* Blueprint,
        const FString& GraphName);

    // ---- AnimGraph (AnimBlueprint-only) ----

    // Adds a State Machine node to the named AnimGraph (an AnimBlueprint's main graph is a
    // FunctionGraph named "AnimGraph", so the existing graph lookup finds it). Auto-creates the
    // machine's inner EditorStateMachineGraph + Entry node. Returns the new node's name, or empty on failure.
    UFUNCTION(BlueprintCallable, Category = "MCP|AnimGraph", meta = (DevelopmentOnly))
    static FString AddAnimStateMachineNode(
        UAnimBlueprint* AnimBlueprint,
        const FString& GraphName,
        int32 PosX,
        int32 PosY);

    // Adds a state to a state machine's inner graph. Auto-creates the state's BoundGraph
    // (with its Output Animation Pose sink). If this is the machine's first state, the Entry
    // node is auto-wired to it. Returns the new state node's name, or empty on failure.
    UFUNCTION(BlueprintCallable, Category = "MCP|AnimGraph", meta = (DevelopmentOnly))
    static FString AddAnimState(
        UAnimBlueprint* AnimBlueprint,
        const FString& GraphName,
        const FString& StateMachineNodeName,
        const FString& StateName,
        int32 PosX,
        int32 PosY);

    // Adds a BlendSpacePlayer node inside StateName's bound graph, sets its BlendSpace asset,
    // and wires its pose output to the state's Output Animation Pose sink. The blend space's
    // axis input pins (typically named X/Y) are left for ConnectPins. Returns the new node's
    // name, or empty on failure.
    UFUNCTION(BlueprintCallable, Category = "MCP|AnimGraph", meta = (DevelopmentOnly))
    static FString AddBlendSpacePlayerToState(
        UAnimBlueprint* AnimBlueprint,
        const FString& GraphName,
        const FString& StateMachineNodeName,
        const FString& StateName,
        const FString& BlendSpaceAssetPath,
        int32 PosX,
        int32 PosY);

    // Adds a plain (non-blend-space) SequencePlayer node inside StateName's bound graph, sets its
    // Sequence asset and loop flag, and wires its pose output to the state's Output Animation Pose
    // sink. Use for one-shot transition animations (Start/Stop clips) inside a dedicated state.
    // Returns the new node's name, or empty on failure.
    UFUNCTION(BlueprintCallable, Category = "MCP|AnimGraph", meta = (DevelopmentOnly))
    static FString AddSequencePlayerToState(
        UAnimBlueprint* AnimBlueprint,
        const FString& GraphName,
        const FString& StateMachineNodeName,
        const FString& StateName,
        const FString& SequenceAssetPath,
        bool bLoop,
        int32 PosX,
        int32 PosY);

    // Adds a Slot node (montage layering point) to the named top-level AnimGraph. SlotName must
    // match a slot defined on the Skeleton (e.g. "UpperBody"); a montage authored on that slot only
    // plays visibly once this node sits in series between the pose source and the output. Input pin
    // "Source", output pin "Pose" (same convention as the other Add*ToState nodes) — wire both with
    // ConnectPins/DisconnectPinLink. Returns the new node's name, or empty on failure.
    UFUNCTION(BlueprintCallable, Category = "MCP|AnimGraph", meta = (DevelopmentOnly))
    static FString AddAnimSlotNode(
        UAnimBlueprint* AnimBlueprint,
        const FString& GraphName,
        const FString& SlotName,
        int32 PosX,
        int32 PosY);

    // Adds a transition between two states in a state machine. If ConditionPropertyName is
    // non-empty, the transition's rule graph is wired to `Get <ConditionPropertyName>` (a bool
    // property on the AnimBlueprint's parent class, e.g. "bIsInAir") -> Result, optionally negated.
    // Returns the new transition node's name, or empty on failure.
    UFUNCTION(BlueprintCallable, Category = "MCP|AnimGraph", meta = (DevelopmentOnly))
    static FString AddAnimTransition(
        UAnimBlueprint* AnimBlueprint,
        const FString& GraphName,
        const FString& StateMachineNodeName,
        const FString& FromStateName,
        const FString& ToStateName,
        const FString& ConditionPropertyName,
        bool bNegateCondition);

    // ---- BlendSpace (asset editing) ----

    // Repositions an existing sample in a BlendSpace/BlendSpace1D asset. Unlike editing the
    // SampleData property directly (which the blend space's internal grid cache silently discards
    // on save), this goes through UBlendSpace::EditSampleValue so the grid is properly rebuilt.
    // SampleIndex is the sample's position in GetSampleData() (0-based, in authoring order).
    UFUNCTION(BlueprintCallable, Category = "MCP|Asset", meta = (DevelopmentOnly))
    static bool EditBlendSpaceSampleValue(
        class UBlendSpace* BlendSpace,
        int32 SampleIndex,
        float X,
        float Y);

    // Sets an axis's Min/Max range on a BlendSpace. BlendParameters is a protected fixed-size C
    // array (not reachable through the generic Python/Blueprint property accessors), so this
    // reaches it via FProperty reflection instead. AxisIndex: 0=X, 1=Y, 2=Z.
    UFUNCTION(BlueprintCallable, Category = "MCP|Asset", meta = (DevelopmentOnly))
    static bool SetBlendSpaceAxisRange(
        class UBlendSpace* BlendSpace,
        int32 AxisIndex,
        float Min,
        float Max);

    // Resolves GraphName against Blueprint's top-level graphs, then (for AnimBlueprints) falls
    // back to nested AnimGraph graphs: state machine inner graphs, state bound graphs, and
    // transition rule graphs. Public so other command handlers (e.g. blueprint.get_graph_nodes)
    // can inspect nested anim graphs too. Not a UFUNCTION — C++-only, no Python/Blueprint exposure.
    static UEdGraph* FindGraphByName(UBlueprint* BP, const FString& GraphName);

private:
    static UEdGraphNode* FindNodeByName(UEdGraph* Graph, const FString& NodeName);
    static UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName);

    // Finds a state (by GetStateName()) in a state machine's inner graph.
    static class UAnimStateNodeBase* FindStateByName(class UAnimGraphNode_StateMachineBase* MachineNode, const FString& StateName);
};
