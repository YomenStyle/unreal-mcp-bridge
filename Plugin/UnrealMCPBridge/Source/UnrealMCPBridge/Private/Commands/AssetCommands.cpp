#include "Commands/AssetCommands.h"
#include "Commands/CommandJsonHelpers.h"
#include "MCPCommandRegistry.h"
#include "MCPProtocol.h"
#include "Dom/JsonObject.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "EditorAssetLibrary.h"

void FAssetCommandHandler::RegisterCommands(FMCPCommandRegistry& Registry)
{
    // asset.list — returns assets under a given content path
    Registry.Register(TEXT("asset.list"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!Params.IsValid())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("params object is required");
                return nullptr;
            }

            FString PackagePath;
            if (!Params->TryGetStringField(TEXT("package_path"), PackagePath) || PackagePath.IsEmpty())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("package_path is required and must be non-empty");
                return nullptr;
            }

            bool bRecursive = false;
            Params->TryGetBoolField(TEXT("recursive"), bRecursive);

            IAssetRegistry& AssetRegistry =
                FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

            TArray<FAssetData> AssetList;
            AssetRegistry.GetAssetsByPath(FName(*PackagePath), AssetList, bRecursive);

            TArray<TSharedPtr<FJsonValue>> AssetArray;
            for (const FAssetData& Asset : AssetList)
            {
                AssetArray.Add(MakeShared<FJsonValueObject>(UnrealMCPBridge::Json::MakeAssetJson(Asset)));
            }

            auto Result = MakeShared<FJsonObject>();
            Result->SetArrayField(TEXT("assets"), AssetArray);
            return Result;
        });

    // asset.get_metadata — returns class and tag map for an asset by object path
    Registry.Register(TEXT("asset.get_metadata"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!Params.IsValid())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("params object is required");
                return nullptr;
            }

            FString ObjectPath;
            if (!Params->TryGetStringField(TEXT("object_path"), ObjectPath) || ObjectPath.IsEmpty())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("object_path is required and must be non-empty");
                return nullptr;
            }

            IAssetRegistry& AssetRegistry =
                FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

            FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
            if (!AssetData.IsValid())
            {
                OutError.Code = MCPProtocol::FMCPError::ServerErrorMin; // -32099: Asset not found
                OutError.Message = FString::Printf(TEXT("Asset not found: %s"), *ObjectPath);
                return nullptr;
            }

            auto TagsObj = MakeShared<FJsonObject>();
            AssetData.TagsAndValues.ForEach([&TagsObj](const TPair<FName, FAssetTagValueRef>& Tag)
            {
                TagsObj->SetStringField(Tag.Key.ToString(), Tag.Value.GetValue());
            });

            auto Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
            Result->SetObjectField(TEXT("tags"),  TagsObj);
            return Result;
        });

    // asset.save — saves an asset package to disk
    Registry.Register(TEXT("asset.save"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!Params.IsValid())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("params object is required");
                return nullptr;
            }

            FString ObjectPath;
            if (!Params->TryGetStringField(TEXT("object_path"), ObjectPath) || ObjectPath.IsEmpty())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("object_path is required and must be non-empty");
                return nullptr;
            }

            // SaveAsset expects a package path (without the asset name after the last dot)
            const bool bSaved = UEditorAssetLibrary::SaveAsset(ObjectPath, /*bOnlyIfIsDirty=*/false);

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("saved"), bSaved);
            return Result;
        });
}
