#include "Commands/BlueprintCommands.h"
#include "Commands/CommandJsonHelpers.h"
#include "MCPCommandRegistry.h"
#include "MCPProtocol.h"
#include "MCPGraphEditLibrary.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"

void FBlueprintCommandHandler::RegisterCommands(FMCPCommandRegistry& Registry)
{
    // blueprint.create — creates a new Blueprint asset under the given package path
    Registry.Register(TEXT("blueprint.create"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!Params.IsValid())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("params object is required");
                return nullptr;
            }

            FString PackagePath;
            FString AssetName;
            FString ParentClassPath;

            if (!Params->TryGetStringField(TEXT("package_path"), PackagePath) || PackagePath.IsEmpty())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("package_path is required and must be non-empty");
                return nullptr;
            }
            if (!Params->TryGetStringField(TEXT("asset_name"), AssetName) || AssetName.IsEmpty())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("asset_name is required and must be non-empty");
                return nullptr;
            }
            if (!Params->TryGetStringField(TEXT("parent_class_path"), ParentClassPath) || ParentClassPath.IsEmpty())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("parent_class_path is required and must be non-empty");
                return nullptr;
            }

            UClass* ParentClass = LoadObject<UClass>(nullptr, *ParentClassPath);
            if (!ParentClass)
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = FString::Printf(TEXT("Could not load parent class: %s"), *ParentClassPath);
                return nullptr;
            }

            // Build the full package name e.g. /Game/Blueprints/BP_MyActor
            const FString FullPackageName = PackagePath / AssetName;
            UPackage* Package = CreatePackage(*FullPackageName);
            if (!Package)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = FString::Printf(TEXT("Could not create package: %s"), *FullPackageName);
                return nullptr;
            }
            Package->FullyLoad();

            UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
                ParentClass, Package, FName(*AssetName), BPTYPE_Normal);

            if (!NewBP)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("FKismetEditorUtilities::CreateBlueprint returned null");
                return nullptr;
            }

            // Notify asset registry of the new asset
            FAssetRegistryModule::AssetCreated(NewBP);
            Package->MarkPackageDirty();

            auto Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("blueprint_path"), NewBP->GetPathName());
            return Result;
        });

    // blueprint.compile — compiles a Blueprint asset by object path
    Registry.Register(TEXT("blueprint.compile"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!Params.IsValid())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("params object is required");
                return nullptr;
            }

            FString BlueprintPath;
            if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) || BlueprintPath.IsEmpty())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("blueprint_path is required and must be non-empty");
                return nullptr;
            }

            UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
            if (!BP)
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = FString::Printf(TEXT("Could not load Blueprint: %s"), *BlueprintPath);
                return nullptr;
            }

            FKismetEditorUtilities::CompileBlueprint(BP);

            const bool  bSuccess     = (BP->Status != BS_Error);
            const int32 ErrorCount   = bSuccess ? 0 : 1;
            // Warning count is not exposed by the public Blueprint status API without KismetCompiler internals.
            const int32 WarningCount = 0;

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("success"), bSuccess);
            Result->SetNumberField(TEXT("error_count"), static_cast<double>(ErrorCount));
            Result->SetNumberField(TEXT("warning_count"), static_cast<double>(WarningCount));
            return Result;
        });

    // blueprint.list_variables — lists NewVariables from a Blueprint
    Registry.Register(TEXT("blueprint.list_variables"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!Params.IsValid())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("params object is required");
                return nullptr;
            }

            FString BlueprintPath;
            if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) || BlueprintPath.IsEmpty())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("blueprint_path is required and must be non-empty");
                return nullptr;
            }

            UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
            if (!BP)
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = FString::Printf(TEXT("Could not load Blueprint: %s"), *BlueprintPath);
                return nullptr;
            }

            TArray<TSharedPtr<FJsonValue>> VarArray;
            for (const FBPVariableDescription& VarDesc : BP->NewVariables)
            {
                // Build a human-readable type string from the pin type
                FString TypeStr;
                const FEdGraphPinType& PinType = VarDesc.VarType;
                if (PinType.PinSubCategoryObject.IsValid())
                {
                    TypeStr = PinType.PinSubCategoryObject->GetName();
                }
                else if (!PinType.PinSubCategory.IsNone())
                {
                    TypeStr = PinType.PinSubCategory.ToString();
                }
                else
                {
                    TypeStr = PinType.PinCategory.ToString();
                }

                VarArray.Add(MakeShared<FJsonValueObject>(
                    UnrealMCPBridge::Json::MakeVariableJson(
                        VarDesc.VarName.ToString(),
                        TypeStr,
                        VarDesc.Category.ToString())));
            }

            auto Result = MakeShared<FJsonObject>();
            Result->SetArrayField(TEXT("variables"), VarArray);
            return Result;
        });

    // blueprint.get_graph_nodes — dumps nodes & pin connections from BP's ubergraph / function / macro graphs.
    // Params: blueprint_path (string, required), graph_name (string, optional — filters by graph name).
    Registry.Register(TEXT("blueprint.get_graph_nodes"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!Params.IsValid())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("params object is required");
                return nullptr;
            }

            FString BlueprintPath;
            if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath) || BlueprintPath.IsEmpty())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("blueprint_path is required and must be non-empty");
                return nullptr;
            }

            FString GraphNameFilter;
            Params->TryGetStringField(TEXT("graph_name"), GraphNameFilter);

            UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
            if (!BP)
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = FString::Printf(TEXT("Could not load Blueprint: %s"), *BlueprintPath);
                return nullptr;
            }

            auto BuildPinJson = [](UEdGraphPin* Pin) -> TSharedPtr<FJsonObject>
            {
                TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
                PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
                PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                if (!Pin->PinType.PinSubCategory.IsNone())
                {
                    PinObj->SetStringField(TEXT("subtype"), Pin->PinType.PinSubCategory.ToString());
                }
                if (Pin->PinType.PinSubCategoryObject.IsValid())
                {
                    PinObj->SetStringField(TEXT("subtype_object"), Pin->PinType.PinSubCategoryObject->GetName());
                }
                if (!Pin->DefaultValue.IsEmpty())
                {
                    PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
                }

                TArray<TSharedPtr<FJsonValue>> Linked;
                for (UEdGraphPin* L : Pin->LinkedTo)
                {
                    if (!L) continue;
                    UEdGraphNode* LNode = L->GetOwningNode();
                    const FString Ref = FString::Printf(TEXT("%s:%s"),
                        LNode ? *LNode->GetName() : TEXT(""),
                        *L->PinName.ToString());
                    Linked.Add(MakeShared<FJsonValueString>(Ref));
                }
                PinObj->SetArrayField(TEXT("connected_to"), Linked);
                return PinObj;
            };

            auto BuildNodeJson = [&BuildPinJson](UEdGraphNode* Node) -> TSharedPtr<FJsonObject>
            {
                TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
                NodeObj->SetStringField(TEXT("name"), Node->GetName());
                NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
                NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
                NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);

                TArray<TSharedPtr<FJsonValue>> Pins;
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (!Pin) continue;
                    Pins.Add(MakeShared<FJsonValueObject>(BuildPinJson(Pin)));
                }
                NodeObj->SetArrayField(TEXT("pins"), Pins);
                return NodeObj;
            };

            auto BuildGraphJson = [&BuildNodeJson](UEdGraph* Graph, const FString& Type) -> TSharedPtr<FJsonObject>
            {
                TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
                GraphObj->SetStringField(TEXT("name"), Graph->GetName());
                GraphObj->SetStringField(TEXT("type"), Type);

                TArray<TSharedPtr<FJsonValue>> Nodes;
                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (!Node) continue;
                    Nodes.Add(MakeShared<FJsonValueObject>(BuildNodeJson(Node)));
                }
                GraphObj->SetArrayField(TEXT("nodes"), Nodes);
                return GraphObj;
            };

            TArray<TSharedPtr<FJsonValue>> AllGraphs;
            auto AddGraphs = [&](const TArray<UEdGraph*>& Graphs, const FString& Type)
            {
                for (UEdGraph* G : Graphs)
                {
                    if (!G) continue;
                    if (!GraphNameFilter.IsEmpty() && G->GetName() != GraphNameFilter) continue;
                    AllGraphs.Add(MakeShared<FJsonValueObject>(BuildGraphJson(G, Type)));
                }
            };

            AddGraphs(BP->UbergraphPages, TEXT("ubergraph"));
            AddGraphs(BP->FunctionGraphs, TEXT("function"));
            AddGraphs(BP->MacroGraphs, TEXT("macro"));

            // Not found among top-level graphs — for AnimBlueprints, GraphNameFilter may name a
            // nested graph (state machine inner graph, state bound graph, transition rule graph).
            if (AllGraphs.Num() == 0 && !GraphNameFilter.IsEmpty())
            {
                if (UEdGraph* Nested = UMCPGraphEditLibrary::FindGraphByName(BP, GraphNameFilter))
                {
                    AllGraphs.Add(MakeShared<FJsonValueObject>(BuildGraphJson(Nested, TEXT("nested"))));
                }
            }

            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetArrayField(TEXT("graphs"), AllGraphs);
            return Result;
        });
}
