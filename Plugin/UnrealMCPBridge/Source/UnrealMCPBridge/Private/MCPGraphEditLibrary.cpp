#include "MCPGraphEditLibrary.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealMCPBridgeModule.h"

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
    Node->BreakAllNodeLinks();
    Graph->RemoveNode(Node);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();
    return true;
}

