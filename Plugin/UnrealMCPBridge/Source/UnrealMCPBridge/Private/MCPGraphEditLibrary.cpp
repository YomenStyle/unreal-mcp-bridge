#include "MCPGraphEditLibrary.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UnrealMCPBridgeModule.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimationStateMachineGraph.h"
#include "AnimStateNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_TransitionResult.h"

UEdGraph* UMCPGraphEditLibrary::FindGraphByName(UBlueprint* BP, const FString& GraphName)
{
    if (!BP) return nullptr;

    auto Search = [&](const TArray<UEdGraph*>& Graphs) -> UEdGraph*
    {
        for (UEdGraph* G : Graphs)
        {
            if (G && G->GetName() == GraphName) return G;
        }
        return nullptr;
    };

    if (UEdGraph* G = Search(BP->UbergraphPages)) return G;
    if (UEdGraph* G = Search(BP->FunctionGraphs)) return G;
    if (UEdGraph* G = Search(BP->MacroGraphs))    return G;

    // Not a top-level graph — check nested AnimGraph graphs (state machine inner graphs,
    // state bound graphs, transition rule graphs), which BP's top-level arrays don't include.
    auto SearchStateMachineNested = [&](UEdGraph* TopGraph) -> UEdGraph*
    {
        if (!TopGraph) return nullptr;
        for (UEdGraphNode* Node : TopGraph->Nodes)
        {
            UAnimGraphNode_StateMachineBase* MachineNode = Cast<UAnimGraphNode_StateMachineBase>(Node);
            if (!MachineNode || !MachineNode->EditorStateMachineGraph) continue;

            UAnimationStateMachineGraph* InnerGraph = MachineNode->EditorStateMachineGraph;
            if (InnerGraph->GetName() == GraphName) return InnerGraph;

            for (UEdGraphNode* InnerNode : InnerGraph->Nodes)
            {
                if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(InnerNode))
                {
                    if (StateNode->BoundGraph && StateNode->BoundGraph->GetName() == GraphName)
                    {
                        return StateNode->BoundGraph;
                    }
                }
                else if (UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(InnerNode))
                {
                    if (TransNode->BoundGraph && TransNode->BoundGraph->GetName() == GraphName)
                    {
                        return TransNode->BoundGraph;
                    }
                }
            }
        }
        return nullptr;
    };

    for (UEdGraph* TopGraph : BP->FunctionGraphs)
    {
        if (UEdGraph* Found = SearchStateMachineNested(TopGraph)) return Found;
    }

    return nullptr;
}

UEdGraphNode* UMCPGraphEditLibrary::FindNodeByName(UEdGraph* Graph, const FString& NodeName)
{
    if (!Graph) return nullptr;
    for (UEdGraphNode* N : Graph->Nodes)
    {
        if (N && N->GetName() == NodeName) return N;
    }
    return nullptr;
}

UEdGraphPin* UMCPGraphEditLibrary::FindPinByName(UEdGraphNode* Node, const FString& PinName)
{
    if (!Node) return nullptr;
    for (UEdGraphPin* P : Node->Pins)
    {
        if (P && P->PinName.ToString() == PinName) return P;
    }
    return nullptr;
}

FString UMCPGraphEditLibrary::AddCallFunctionNode(
    UBlueprint* Blueprint,
    const FString& GraphName,
    const FString& FunctionClassPath,
    const FString& FunctionName,
    int32 PosX,
    int32 PosY)
{
    if (!Blueprint)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddCallFunctionNode: Blueprint is null"));
        return FString();
    }

    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddCallFunctionNode: graph '%s' not found"), *GraphName);
        return FString();
    }

    UClass* FuncClass = LoadObject<UClass>(nullptr, *FunctionClassPath);
    if (!FuncClass)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddCallFunctionNode: class '%s' not found"), *FunctionClassPath);
        return FString();
    }

    UFunction* Func = FuncClass->FindFunctionByName(FName(*FunctionName));
    if (!Func)
    {
        UE_LOG(LogMCPBridge, Warning,
            TEXT("AddCallFunctionNode: function '%s' not found on '%s'"),
            *FunctionName, *FunctionClassPath);
        return FString();
    }

    UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
    Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
    Node->CreateNewGuid();
    Node->SetFromFunction(Func);
    Node->NodePosX = PosX;
    Node->NodePosY = PosY;
    Node->AllocateDefaultPins();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return Node->GetName();
}

FString UMCPGraphEditLibrary::AddIfThenElseNode(
    UBlueprint* Blueprint,
    const FString& GraphName,
    int32 PosX,
    int32 PosY)
{
    if (!Blueprint)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddIfThenElseNode: Blueprint is null"));
        return FString();
    }

    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddIfThenElseNode: graph '%s' not found"), *GraphName);
        return FString();
    }

    UK2Node_IfThenElse* Node = NewObject<UK2Node_IfThenElse>(Graph);
    Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
    Node->CreateNewGuid();
    Node->NodePosX = PosX;
    Node->NodePosY = PosY;
    Node->AllocateDefaultPins();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return Node->GetName();
}

FString UMCPGraphEditLibrary::AddVariableGetNode(
    UBlueprint* Blueprint,
    const FString& GraphName,
    const FString& PropertyName,
    int32 PosX,
    int32 PosY)
{
    if (!Blueprint)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddVariableGetNode: Blueprint is null"));
        return FString();
    }

    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddVariableGetNode: graph '%s' not found"), *GraphName);
        return FString();
    }

    UClass* InstanceClass = Blueprint->GeneratedClass ? Blueprint->GeneratedClass : Blueprint->ParentClass;
    FProperty* Prop = InstanceClass ? InstanceClass->FindPropertyByName(FName(*PropertyName)) : nullptr;
    if (!Prop)
    {
        UE_LOG(LogMCPBridge, Warning,
            TEXT("AddVariableGetNode: property '%s' not found on %s"),
            *PropertyName, InstanceClass ? *InstanceClass->GetName() : TEXT("<null>"));
        return FString();
    }

    UK2Node_VariableGet* Node = NewObject<UK2Node_VariableGet>(Graph);
    Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
    Node->CreateNewGuid();
    Node->VariableReference.SetSelfMember(Prop->GetFName());
    Node->NodePosX = PosX;
    Node->NodePosY = PosY;
    Node->AllocateDefaultPins();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return Node->GetName();
}

bool UMCPGraphEditLibrary::ConnectPins(
    UBlueprint* Blueprint,
    const FString& GraphName,
    const FString& SrcNodeName,
    const FString& SrcPinName,
    const FString& DstNodeName,
    const FString& DstPinName)
{
    if (!Blueprint) return false;
    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph) return false;

    UEdGraphNode* SrcNode = FindNodeByName(Graph, SrcNodeName);
    UEdGraphNode* DstNode = FindNodeByName(Graph, DstNodeName);
    if (!SrcNode || !DstNode)
    {
        UE_LOG(LogMCPBridge, Warning,
            TEXT("ConnectPins: node not found (src=%s found=%d, dst=%s found=%d)"),
            *SrcNodeName, SrcNode != nullptr, *DstNodeName, DstNode != nullptr);
        return false;
    }

    UEdGraphPin* SrcPin = FindPinByName(SrcNode, SrcPinName);
    UEdGraphPin* DstPin = FindPinByName(DstNode, DstPinName);
    if (!SrcPin || !DstPin)
    {
        UE_LOG(LogMCPBridge, Warning,
            TEXT("ConnectPins: pin not found (src=%s.%s found=%d, dst=%s.%s found=%d)"),
            *SrcNodeName, *SrcPinName, SrcPin != nullptr,
            *DstNodeName, *DstPinName, DstPin != nullptr);
        return false;
    }

    SrcPin->MakeLinkTo(DstPin);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool UMCPGraphEditLibrary::DisconnectPinLink(
    UBlueprint* Blueprint,
    const FString& GraphName,
    const FString& NodeA,
    const FString& PinA,
    const FString& NodeB,
    const FString& PinB)
{
    if (!Blueprint) return false;
    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph) return false;

    UEdGraphNode* NA = FindNodeByName(Graph, NodeA);
    UEdGraphNode* NB = FindNodeByName(Graph, NodeB);
    if (!NA || !NB) return false;

    UEdGraphPin* PA = FindPinByName(NA, PinA);
    UEdGraphPin* PB = FindPinByName(NB, PinB);
    if (!PA || !PB) return false;

    PA->BreakLinkTo(PB);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool UMCPGraphEditLibrary::EditBlendSpaceSampleValue(
    UBlendSpace* BlendSpace,
    int32 SampleIndex,
    float X,
    float Y)
{
    if (!BlendSpace) return false;

    BlendSpace->Modify();
    const bool bOk = BlendSpace->EditSampleValue(SampleIndex, FVector(X, Y, 0.f));
    if (bOk)
    {
        BlendSpace->PostEditChange();
        BlendSpace->MarkPackageDirty();
    }
    return bOk;
}

bool UMCPGraphEditLibrary::SetBlendSpaceAxisRange(
    UBlendSpace* BlendSpace,
    int32 AxisIndex,
    float Min,
    float Max)
{
    if (!BlendSpace || AxisIndex < 0 || AxisIndex > 2) return false;

    // BlendParameters is protected, so go through FProperty reflection (which ignores C++ access
    // specifiers) rather than needing a public setter that doesn't exist on UBlendSpace.
    FStructProperty* Prop = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
    if (!Prop) return false;

    FBlendParameter* Param = Prop->ContainerPtrToValuePtr<FBlendParameter>(BlendSpace, AxisIndex);
    if (!Param) return false;

    BlendSpace->Modify();
    Param->Min = Min;
    Param->Max = Max;
    BlendSpace->ValidateSampleData();
    BlendSpace->PostEditChange();
    BlendSpace->MarkPackageDirty();
    return true;
}

bool UMCPGraphEditLibrary::CompileBlueprint(UBlueprint* Blueprint)
{
    if (!Blueprint) return false;
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error;
}

bool UMCPGraphEditLibrary::RemoveNode(
    UBlueprint* Blueprint,
    const FString& GraphName,
    const FString& NodeName)
{
    if (!Blueprint) return false;
    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph) return false;
    UEdGraphNode* Node = FindNodeByName(Graph, NodeName);
    if (!Node) return false;
    Node->Modify();
    Graph->Modify();
    // DestroyNode() (not a manual BreakAllNodeLinks+RemoveNode) so node-type-specific cleanup runs —
    // e.g. UAnimGraphNode_StateMachineBase::DestroyNode() also removes its EditorStateMachineGraph
    // via FBlueprintEditorUtils::RemoveGraph, which a plain Graph->RemoveNode() would silently skip
    // (leaving an orphaned inner graph behind).
    Node->DestroyNode();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

bool UMCPGraphEditLibrary::RemoveOrphanedGraph(
    UBlueprint* Blueprint,
    const FString& GraphName)
{
    if (!Blueprint) return false;

    UEdGraph* Found = nullptr;
    ForEachObjectWithOuter(Blueprint, [&](UObject* Obj)
    {
        if (Found) return;
        if (UEdGraph* G = Cast<UEdGraph>(Obj))
        {
            if (G->GetName() == GraphName)
            {
                Found = G;
            }
        }
    }, EGetObjectsFlags::IncludeNestedObjects);

    if (!Found)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("RemoveOrphanedGraph: no graph named '%s' found under %s"), *GraphName, *Blueprint->GetName());
        return false;
    }

    Found->Modify();
    FBlueprintEditorUtils::RemoveGraph(Blueprint, Found, EGraphRemoveFlags::Recompile);
    Blueprint->MarkPackageDirty();
    return true;
}

// ---- AnimGraph ----

UAnimStateNodeBase* UMCPGraphEditLibrary::FindStateByName(UAnimGraphNode_StateMachineBase* MachineNode, const FString& StateName)
{
    if (!MachineNode || !MachineNode->EditorStateMachineGraph) return nullptr;
    for (UEdGraphNode* N : MachineNode->EditorStateMachineGraph->Nodes)
    {
        if (UAnimStateNodeBase* StateNode = Cast<UAnimStateNodeBase>(N))
        {
            if (StateNode->GetStateName() == StateName)
            {
                return StateNode;
            }
        }
    }
    return nullptr;
}

FString UMCPGraphEditLibrary::AddAnimStateMachineNode(
    UAnimBlueprint* AnimBlueprint,
    const FString& GraphName,
    int32 PosX,
    int32 PosY)
{
    if (!AnimBlueprint)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddAnimStateMachineNode: AnimBlueprint is null"));
        return FString();
    }

    UEdGraph* Graph = FindGraphByName(AnimBlueprint, GraphName);
    if (!Graph)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddAnimStateMachineNode: graph '%s' not found"), *GraphName);
        return FString();
    }

    UAnimGraphNode_StateMachine* Node = NewObject<UAnimGraphNode_StateMachine>(Graph);
    Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
    Node->CreateNewGuid();
    Node->NodePosX = PosX;
    Node->NodePosY = PosY;
    Node->AllocateDefaultPins();
    Node->PostPlacedNewNode(); // creates EditorStateMachineGraph + Entry node

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
    AnimBlueprint->MarkPackageDirty();
    return Node->GetName();
}

FString UMCPGraphEditLibrary::AddAnimState(
    UAnimBlueprint* AnimBlueprint,
    const FString& GraphName,
    const FString& StateMachineNodeName,
    const FString& StateName,
    int32 PosX,
    int32 PosY)
{
    if (!AnimBlueprint)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddAnimState: AnimBlueprint is null"));
        return FString();
    }

    UEdGraph* Graph = FindGraphByName(AnimBlueprint, GraphName);
    if (!Graph) return FString();

    UAnimGraphNode_StateMachineBase* MachineNode = Cast<UAnimGraphNode_StateMachineBase>(FindNodeByName(Graph, StateMachineNodeName));
    if (!MachineNode || !MachineNode->EditorStateMachineGraph)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddAnimState: state machine '%s' not found or has no inner graph"), *StateMachineNodeName);
        return FString();
    }

    UAnimationStateMachineGraph* InnerGraph = MachineNode->EditorStateMachineGraph;
    const bool bIsFirstState = InnerGraph->EntryNode
        && InnerGraph->EntryNode->GetOutputPin()
        && InnerGraph->EntryNode->GetOutputPin()->LinkedTo.Num() == 0;

    UAnimStateNode* StateNode = NewObject<UAnimStateNode>(InnerGraph);
    InnerGraph->AddNode(StateNode, false, false);
    StateNode->CreateNewGuid();
    StateNode->NodePosX = PosX;
    StateNode->NodePosY = PosY;
    StateNode->AllocateDefaultPins();
    StateNode->PostPlacedNewNode(); // creates BoundGraph + Result sink
    StateNode->OnRenameNode(StateName);

    // Give the state's inner (bound) graph a predictable name matching StateName, so callers
    // can address it directly via GraphName (e.g. for AddBlendSpacePlayerToState, AddVariableGetNode,
    // ConnectPins) instead of having to discover an auto-generated graph name.
    if (StateNode->BoundGraph && StateNode->BoundGraph->GetName() != StateName)
    {
        StateNode->BoundGraph->Rename(*StateName, nullptr, REN_DontCreateRedirectors);
    }

    if (bIsFirstState && InnerGraph->EntryNode && InnerGraph->EntryNode->GetOutputPin())
    {
        InnerGraph->EntryNode->GetOutputPin()->MakeLinkTo(StateNode->GetInputPin());
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
    AnimBlueprint->MarkPackageDirty();
    return StateNode->GetName();
}

FString UMCPGraphEditLibrary::AddBlendSpacePlayerToState(
    UAnimBlueprint* AnimBlueprint,
    const FString& GraphName,
    const FString& StateMachineNodeName,
    const FString& StateName,
    const FString& BlendSpaceAssetPath,
    int32 PosX,
    int32 PosY)
{
    if (!AnimBlueprint) return FString();
    UEdGraph* Graph = FindGraphByName(AnimBlueprint, GraphName);
    if (!Graph) return FString();

    UAnimGraphNode_StateMachineBase* MachineNode = Cast<UAnimGraphNode_StateMachineBase>(FindNodeByName(Graph, StateMachineNodeName));
    if (!MachineNode)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddBlendSpacePlayerToState: state machine '%s' not found"), *StateMachineNodeName);
        return FString();
    }

    UAnimStateNode* StateNode = Cast<UAnimStateNode>(FindStateByName(MachineNode, StateName));
    if (!StateNode || !StateNode->BoundGraph)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddBlendSpacePlayerToState: state '%s' not found or has no bound graph"), *StateName);
        return FString();
    }

    UBlendSpace* BlendSpaceAsset = LoadObject<UBlendSpace>(nullptr, *BlendSpaceAssetPath);
    if (!BlendSpaceAsset)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddBlendSpacePlayerToState: could not load blend space '%s'"), *BlendSpaceAssetPath);
        return FString();
    }

    UAnimGraphNode_BlendSpacePlayer* BSNode = NewObject<UAnimGraphNode_BlendSpacePlayer>(StateNode->BoundGraph);
    StateNode->BoundGraph->AddNode(BSNode, false, false);
    BSNode->CreateNewGuid();
    BSNode->NodePosX = PosX;
    BSNode->NodePosY = PosY;
    BSNode->Node.SetBlendSpace(BlendSpaceAsset);
    BSNode->AllocateDefaultPins();

    UEdGraphPin* PoseSink = StateNode->GetPoseSinkPinInsideState();
    UEdGraphPin* PoseOutput = FindPinByName(BSNode, TEXT("Pose"));
    if (PoseSink && PoseOutput)
    {
        PoseOutput->MakeLinkTo(PoseSink);
    }
    else
    {
        UE_LOG(LogMCPBridge, Warning,
            TEXT("AddBlendSpacePlayerToState: could not connect Pose pins (sink=%d output=%d) — connect manually with ConnectPins"),
            PoseSink != nullptr, PoseOutput != nullptr);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
    AnimBlueprint->MarkPackageDirty();
    return BSNode->GetName();
}

FString UMCPGraphEditLibrary::AddSequencePlayerToState(
    UAnimBlueprint* AnimBlueprint,
    const FString& GraphName,
    const FString& StateMachineNodeName,
    const FString& StateName,
    const FString& SequenceAssetPath,
    bool bLoop,
    int32 PosX,
    int32 PosY)
{
    if (!AnimBlueprint) return FString();
    UEdGraph* Graph = FindGraphByName(AnimBlueprint, GraphName);
    if (!Graph) return FString();

    UAnimGraphNode_StateMachineBase* MachineNode = Cast<UAnimGraphNode_StateMachineBase>(FindNodeByName(Graph, StateMachineNodeName));
    if (!MachineNode)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddSequencePlayerToState: state machine '%s' not found"), *StateMachineNodeName);
        return FString();
    }

    UAnimStateNode* StateNode = Cast<UAnimStateNode>(FindStateByName(MachineNode, StateName));
    if (!StateNode || !StateNode->BoundGraph)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddSequencePlayerToState: state '%s' not found or has no bound graph"), *StateName);
        return FString();
    }

    UAnimSequenceBase* SequenceAsset = LoadObject<UAnimSequenceBase>(nullptr, *SequenceAssetPath);
    if (!SequenceAsset)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddSequencePlayerToState: could not load sequence '%s'"), *SequenceAssetPath);
        return FString();
    }

    UAnimGraphNode_SequencePlayer* SeqNode = NewObject<UAnimGraphNode_SequencePlayer>(StateNode->BoundGraph);
    StateNode->BoundGraph->AddNode(SeqNode, false, false);
    SeqNode->CreateNewGuid();
    SeqNode->NodePosX = PosX;
    SeqNode->NodePosY = PosY;
    SeqNode->Node.SetSequence(SequenceAsset);
    SeqNode->Node.SetLoopAnimation(bLoop);
    SeqNode->AllocateDefaultPins();

    UEdGraphPin* PoseSink = StateNode->GetPoseSinkPinInsideState();
    UEdGraphPin* PoseOutput = FindPinByName(SeqNode, TEXT("Pose"));
    if (PoseSink && PoseOutput)
    {
        PoseOutput->MakeLinkTo(PoseSink);
    }
    else
    {
        UE_LOG(LogMCPBridge, Warning,
            TEXT("AddSequencePlayerToState: could not connect Pose pins (sink=%d output=%d) — connect manually with ConnectPins"),
            PoseSink != nullptr, PoseOutput != nullptr);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
    AnimBlueprint->MarkPackageDirty();
    return SeqNode->GetName();
}

FString UMCPGraphEditLibrary::AddAnimSlotNode(
    UAnimBlueprint* AnimBlueprint,
    const FString& GraphName,
    const FString& SlotName,
    int32 PosX,
    int32 PosY)
{
    if (!AnimBlueprint) return FString();
    UEdGraph* Graph = FindGraphByName(AnimBlueprint, GraphName);
    if (!Graph) return FString();

    UAnimGraphNode_Slot* SlotNode = NewObject<UAnimGraphNode_Slot>(Graph);
    Graph->AddNode(SlotNode, false, false);
    SlotNode->CreateNewGuid();
    SlotNode->NodePosX = PosX;
    SlotNode->NodePosY = PosY;
    SlotNode->Node.SlotName = FName(*SlotName);
    SlotNode->AllocateDefaultPins();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
    AnimBlueprint->MarkPackageDirty();
    return SlotNode->GetName();
}

FString UMCPGraphEditLibrary::AddAnimTransition(
    UAnimBlueprint* AnimBlueprint,
    const FString& GraphName,
    const FString& StateMachineNodeName,
    const FString& FromStateName,
    const FString& ToStateName,
    const FString& ConditionPropertyName,
    bool bNegateCondition)
{
    if (!AnimBlueprint) return FString();
    UEdGraph* Graph = FindGraphByName(AnimBlueprint, GraphName);
    if (!Graph) return FString();

    UAnimGraphNode_StateMachineBase* MachineNode = Cast<UAnimGraphNode_StateMachineBase>(FindNodeByName(Graph, StateMachineNodeName));
    if (!MachineNode || !MachineNode->EditorStateMachineGraph)
    {
        UE_LOG(LogMCPBridge, Warning, TEXT("AddAnimTransition: state machine '%s' not found or has no inner graph"), *StateMachineNodeName);
        return FString();
    }

    UAnimationStateMachineGraph* InnerGraph = MachineNode->EditorStateMachineGraph;

    UAnimStateNodeBase* FromState = FindStateByName(MachineNode, FromStateName);
    UAnimStateNodeBase* ToState = FindStateByName(MachineNode, ToStateName);
    if (!FromState || !ToState)
    {
        UE_LOG(LogMCPBridge, Warning,
            TEXT("AddAnimTransition: from/to state not found (from=%d to=%d)"),
            FromState != nullptr, ToState != nullptr);
        return FString();
    }

    UAnimStateTransitionNode* TransNode = NewObject<UAnimStateTransitionNode>(InnerGraph);
    InnerGraph->AddNode(TransNode, false, false);
    TransNode->CreateNewGuid();
    TransNode->NodePosX = (FromState->NodePosX + ToState->NodePosX) / 2;
    TransNode->NodePosY = (FromState->NodePosY + ToState->NodePosY) / 2;
    TransNode->AllocateDefaultPins();
    TransNode->PostPlacedNewNode(); // creates BoundGraph (bool rule graph) + TransitionResult sink

    if (UEdGraphPin* FromOut = FromState->GetOutputPin())
    {
        FromOut->MakeLinkTo(TransNode->GetInputPin());
    }
    if (UEdGraphPin* ToIn = ToState->GetInputPin())
    {
        TransNode->GetOutputPin()->MakeLinkTo(ToIn);
    }

    if (!ConditionPropertyName.IsEmpty() && TransNode->BoundGraph)
    {
        UAnimGraphNode_TransitionResult* ResultNode = nullptr;
        for (UEdGraphNode* N : TransNode->BoundGraph->Nodes)
        {
            ResultNode = Cast<UAnimGraphNode_TransitionResult>(N);
            if (ResultNode) break;
        }

        UClass* InstanceClass = AnimBlueprint->ParentClass;
        FProperty* Prop = InstanceClass ? InstanceClass->FindPropertyByName(FName(*ConditionPropertyName)) : nullptr;

        if (ResultNode && Prop)
        {
            UEdGraphPin* ResultBoolPin = FindPinByName(ResultNode, TEXT("bCanEnterTransition"));

            UK2Node_VariableGet* GetterNode = NewObject<UK2Node_VariableGet>(TransNode->BoundGraph);
            TransNode->BoundGraph->AddNode(GetterNode, false, false);
            GetterNode->CreateNewGuid();
            GetterNode->VariableReference.SetSelfMember(Prop->GetFName());
            GetterNode->NodePosX = ResultNode->NodePosX - 300;
            GetterNode->NodePosY = ResultNode->NodePosY;
            GetterNode->AllocateDefaultPins();

            UEdGraphPin* GetterValuePin = FindPinByName(GetterNode, ConditionPropertyName);

            if (ResultBoolPin && GetterValuePin)
            {
                if (bNegateCondition)
                {
                    UK2Node_CallFunction* NotNode = NewObject<UK2Node_CallFunction>(TransNode->BoundGraph);
                    TransNode->BoundGraph->AddNode(NotNode, false, false);
                    NotNode->CreateNewGuid();
                    UFunction* NotFunc = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Not_PreBool"));
                    if (NotFunc)
                    {
                        NotNode->SetFromFunction(NotFunc);
                        NotNode->NodePosX = ResultNode->NodePosX - 150;
                        NotNode->NodePosY = ResultNode->NodePosY;
                        NotNode->AllocateDefaultPins();
                        UEdGraphPin* NotInput = FindPinByName(NotNode, TEXT("A"));
                        UEdGraphPin* NotOutput = FindPinByName(NotNode, TEXT("ReturnValue"));
                        if (NotInput && NotOutput)
                        {
                            GetterValuePin->MakeLinkTo(NotInput);
                            NotOutput->MakeLinkTo(ResultBoolPin);
                        }
                    }
                    else
                    {
                        UE_LOG(LogMCPBridge, Warning, TEXT("AddAnimTransition: could not find Not_PreBool function"));
                    }
                }
                else
                {
                    GetterValuePin->MakeLinkTo(ResultBoolPin);
                }
            }
            else
            {
                UE_LOG(LogMCPBridge, Warning,
                    TEXT("AddAnimTransition: could not wire condition (result_pin=%d getter_pin=%d)"),
                    ResultBoolPin != nullptr, GetterValuePin != nullptr);
            }
        }
        else
        {
            UE_LOG(LogMCPBridge, Warning,
                TEXT("AddAnimTransition: condition property '%s' not found on %s, or no TransitionResult node"),
                *ConditionPropertyName, InstanceClass ? *InstanceClass->GetName() : TEXT("<null>"));
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
    AnimBlueprint->MarkPackageDirty();
    return TransNode->GetName();
}

