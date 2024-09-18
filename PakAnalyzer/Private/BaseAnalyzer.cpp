#include "BaseAnalyzer.h"

#include "AssetRegistry/AssetRegistryState.h"
#include "Json.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/ArrayReader.h"

#include "CommonDefines.h"

FBaseAnalyzer::FBaseAnalyzer()
{

}

FBaseAnalyzer::~FBaseAnalyzer()
{

}

bool FBaseAnalyzer::LoadPakFiles(const TArray<FString>& InPakPaths, const TArray<FString>& InDefaultAESKeys, int32 ContainerStartIndex)
{
	Reset();
	return false;
}

void FBaseAnalyzer::GetFiles(const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, const TMap<int32, bool>& InPakIndexFilter, TArray<FPakFileEntryPtr>& OutFiles) const
{
	FScopeLock Lock(const_cast<FCriticalSection*>(&CriticalSection));

	for (FPakTreeEntryPtr PakTreeRoot : PakTreeRoots)
	{
		RetriveFiles(PakTreeRoot, InFilterText, InClassFilterMap, InPakIndexFilter, OutFiles);
	}
}

const TArray<FPakFileSumaryPtr>& FBaseAnalyzer::GetPakFileSumary() const
{
	return PakFileSummaries;
}

const TArray<FPakTreeEntryPtr>& FBaseAnalyzer::GetPakTreeRootNode() const
{
	return PakTreeRoots;
}

bool FBaseAnalyzer::LoadAssetRegistry(const FString& InRegristryPath)
{
	FArrayReader ContentReader;
	if (!FFileHelper::LoadFileToArray(ContentReader, *InRegristryPath))
	{
		return false;
	}

	if (!LoadAssetRegistry(ContentReader))
	{
		return false;
	}

	AssetRegistryPath = FPaths::ConvertRelativePathToFull(InRegristryPath);

	for (FPakTreeEntryPtr TreeRoot : PakTreeRoots)
	{
		RefreshClassMap(TreeRoot, TreeRoot);
		RefreshPackageDependency(TreeRoot, TreeRoot);
	}
	
	return true;
}

bool FBaseAnalyzer::LoadAssetRegistry(FArrayReader& InData)
{
	FAssetRegistrySerializationOptions LoadOptions;
	LoadOptions.bSerializeDependencies = true;
	LoadOptions.bSerializeSearchableNameDependencies = true;
	LoadOptions.bSerializeManageDependencies = true;
	LoadOptions.bSerializePackageData = false;

	TSharedPtr<FAssetRegistryState> NewAssetRegistryState = MakeShared<FAssetRegistryState>();
	if (NewAssetRegistryState->Serialize(InData, LoadOptions))
	{
		AssetRegistryState = NewAssetRegistryState;
		return true;
	}

	return false;
}

void FBaseAnalyzer::RefreshPackageDependency(FPakTreeEntryPtr InTreeRoot, FPakTreeEntryPtr InRoot)
{
	if (!AssetRegistryState.IsValid())
	{
		return;
	}

	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;

		if (Child->bIsDirectory)
		{
			RefreshPackageDependency(InTreeRoot, Child);
		}
		else
		{
			TArray<FAssetIdentifier> Dependencies;
			if (AssetRegistryState->GetDependencies(Child->PackagePath, Dependencies, UE::AssetRegistry::EDependencyCategory::All))
			{
				if (!Child->AssetSummary.IsValid())
				{
					Child->AssetSummary = MakeShared<FAssetSummary>();
				}
				else
				{
					Child->AssetSummary->DependencyList.Empty();
				}

				for (const FAssetIdentifier& Identifier : Dependencies)
				{
					FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
					Depends->PackageName = Identifier.PackageName;

					Child->AssetSummary->DependencyList.Add(Depends);
				}
			}

			TArray<FAssetIdentifier> Dependents;
			if (AssetRegistryState->GetReferencers(Child->PackagePath, Dependents, UE::AssetRegistry::EDependencyCategory::All))
			{
				if (!Child->AssetSummary.IsValid())
				{
					Child->AssetSummary = MakeShared<FAssetSummary>();
				}
				else
				{
					Child->AssetSummary->DependentList.Empty();
				}

				for (const FAssetIdentifier& Identifier : Dependents)
				{
					FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
					Depends->PackageName = Identifier.PackageName;

					Child->AssetSummary->DependentList.Add(Depends);
				}
			}
		}
	}
}

bool FBaseAnalyzer::ExportToJson(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles)
{
	UE_LOG(LogPakAnalyzer, Log, TEXT("Export to json: %s."), *InOutputPath);

	TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	RootObject->SetNumberField(TEXT("Exported File Count"), InFiles.Num());

	TArray<TSharedPtr<FJsonValue>> FileObjects;

	int64 TotalSize = 0;
	int64 TotalCompressedSize = 0;

	TMap<FName, FPakClassEntry> ExportedClassMap;

	for (const FPakFileEntryPtr It : InFiles)
	{
		const FPakEntry& PakEntry = It->PakEntry;

		TSharedRef<FJsonObject> FileObject = MakeShareable(new FJsonObject);

		FileObject->SetStringField(TEXT("Name"), It->Filename.ToString());
		FileObject->SetStringField(TEXT("Path"), It->Path);
		FileObject->SetNumberField(TEXT("Offset"), PakEntry.Offset);
		FileObject->SetNumberField(TEXT("Size"), PakEntry.UncompressedSize);
		FileObject->SetNumberField(TEXT("Compressed Size"), PakEntry.Size);
		FileObject->SetNumberField(TEXT("Compressed Block Count"), PakEntry.CompressionBlocks.Num());
		FileObject->SetNumberField(TEXT("Compressed Block Size"), PakEntry.CompressionBlockSize);
		FileObject->SetStringField(TEXT("SHA1"), BytesToHex(PakEntry.Hash, sizeof(PakEntry.Hash)));
		FileObject->SetStringField(TEXT("IsEncrypted"), PakEntry.IsEncrypted() ? TEXT("True") : TEXT("False"));
		FileObject->SetStringField(TEXT("Class"), It->Class.ToString());
		FileObject->SetNumberField(TEXT("Dependency Count"), It->AssetSummary.IsValid() ? It->AssetSummary->DependencyList.Num() : 0);
		FileObject->SetNumberField(TEXT("Dependent Count"), It->AssetSummary.IsValid() ? It->AssetSummary->DependentList.Num() : 0);
		FileObject->SetStringField(TEXT("OwnerPak"), PakFileSummaries.IsValidIndex(It->OwnerPakIndex) ? FPaths::GetCleanFilename(PakFileSummaries[It->OwnerPakIndex]->PakFilePath) : TEXT(""));

		FileObjects.Add(MakeShareable(new FJsonValueObject(FileObject)));

		TotalSize += PakEntry.UncompressedSize;
		TotalCompressedSize += PakEntry.Size;

		FPakClassEntry* ClassEntry = ExportedClassMap.Find(It->Class);
		if (ClassEntry)
		{
			ClassEntry->FileCount += 1;
			ClassEntry->CompressedSize += PakEntry.Size;
			ClassEntry->Size += PakEntry.UncompressedSize;
		}
		else
		{
			ExportedClassMap.Add(It->Class, FPakClassEntry(It->Class, PakEntry.UncompressedSize, PakEntry.Size, 1));
		}
	}

	RootObject->SetNumberField(TEXT("Exported Total Size"), TotalSize);
	RootObject->SetNumberField(TEXT("Exported Total Compressed Size"), TotalCompressedSize);

	ExportedClassMap.ValueSort(
		[](const FPakClassEntry& A, const FPakClassEntry& B) -> bool
		{
			return A.CompressedSize > B.CompressedSize;
		});

	TArray<TSharedPtr<FJsonValue>> ClassObjects;
	for (const auto& Pair : ExportedClassMap)
	{
		const FPakClassEntry& ClassEntry = Pair.Value;

		TSharedRef<FJsonObject> ClassObject = MakeShareable(new FJsonObject);
		ClassObject->SetStringField(TEXT("Class"), ClassEntry.Class.ToString());
		ClassObject->SetNumberField(TEXT("File Count"), ClassEntry.FileCount);
		ClassObject->SetNumberField(TEXT("Size"), ClassEntry.Size);
		ClassObject->SetNumberField(TEXT("Compressed Size"), ClassEntry.CompressedSize);
		ClassObject->SetNumberField(TEXT("Compressed Size Percent Of Exported"), TotalCompressedSize > 0 ? 100 * (float)ClassEntry.CompressedSize / TotalCompressedSize : 0.f);

		ClassObjects.Add(MakeShareable(new FJsonValueObject(ClassObject)));
	}

	RootObject->SetArrayField(TEXT("Group By Class"), ClassObjects);
	RootObject->SetArrayField(TEXT("Files"), FileObjects);

	bool bExportResult = false;

	FString FileContents;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&FileContents);
	if (FJsonSerializer::Serialize(RootObject, JsonWriter))
	{
		JsonWriter->Close();
		bExportResult = FFileHelper::SaveStringToFile(FileContents, *InOutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	UE_LOG(LogPakAnalyzer, Log, TEXT("Export to json: %s finished, file count: %d, result: %d."), *InOutputPath, InFiles.Num(), bExportResult);

	return bExportResult;
}

bool FBaseAnalyzer::ExportToCsv(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles)
{
	UE_LOG(LogPakAnalyzer, Log, TEXT("Export to csv: %s."), *InOutputPath);

	TArray<FString> Lines;
	Lines.Empty(InFiles.Num() + 2);
	Lines.Add(TEXT("Id, Name, Path, Offset, Class, Size, Compressed Size, Compressed Block Count, Compressed Block Size, SHA1, IsEncrypted, Dependency Count, Dependent Count, OwnerPak"));

	int32 Index = 1;
	for (const FPakFileEntryPtr It : InFiles)
	{
		const FPakEntry& PakEntry = It->PakEntry;

		Lines.Add(FString::Printf(TEXT("%d, %s, %s, %lld, %s, %lld, %lld, %d, %d, %s, %s, %d, %d, %s"),
			Index,
			*It->Filename.ToString(),
			*It->Path,
			PakEntry.Offset,
			*It->Class.ToString(),
			PakEntry.UncompressedSize,
			PakEntry.Size,
			PakEntry.CompressionBlocks.Num(),
			PakEntry.CompressionBlockSize,
			*BytesToHex(PakEntry.Hash, sizeof(PakEntry.Hash)),
			PakEntry.IsEncrypted() ? TEXT("True") : TEXT("False"),
			It->AssetSummary.IsValid() ? It->AssetSummary->DependencyList.Num() : 0,
			It->AssetSummary.IsValid() ? It->AssetSummary->DependentList.Num() : 0,
			PakFileSummaries.IsValidIndex(It->OwnerPakIndex) ? *FPaths::GetCleanFilename(PakFileSummaries[It->OwnerPakIndex]->PakFilePath) : TEXT(""))
			);
		++Index;
	}

	const bool bExportResult = FFileHelper::SaveStringArrayToFile(Lines, *InOutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	UE_LOG(LogPakAnalyzer, Log, TEXT("Export to csv: %s finished, file count: %d, result: %d."), *InOutputPath, InFiles.Num(), bExportResult);

	return bExportResult;
}

FString FBaseAnalyzer::GetAssetRegistryPath() const
{
	return AssetRegistryPath;
}

void FBaseAnalyzer::RefreshClassMap(FPakTreeEntryPtr InTreeRoot, FPakTreeEntryPtr InRoot)
{
	InRoot->FileClassMap.Empty();

	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;

		if (Child->bIsDirectory)
		{
			RefreshClassMap(InTreeRoot, Child);
			for (auto& ClassPair : Child->FileClassMap)
			{
				InsertClassInfo(InTreeRoot, InRoot, ClassPair.Key, ClassPair.Value->FileCount, ClassPair.Value->Size, ClassPair.Value->CompressedSize);
			}
		}
		else
		{
			Child->Class = GetAssetClass(Child->Path, Child->PackagePath);
			InsertClassInfo(InTreeRoot, InRoot, Child->Class, 1, Child->Size, Child->CompressedSize);
		}
	}
}

void FBaseAnalyzer::RefreshTreeNode(FPakTreeEntryPtr InRoot)
{
	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;
		if (Child->bIsDirectory)
		{
			RefreshTreeNode(Child);
		}
		else
		{
			Child->FileCount = 1;
			Child->Size = Child->PakEntry.UncompressedSize;
			Child->CompressedSize = Child->PakEntry.Size;
		}

		InRoot->FileCount += Child->FileCount;
		InRoot->Size += Child->Size;
		InRoot->CompressedSize += Child->CompressedSize;
	}

	InRoot->ChildrenMap.ValueSort([](const FPakTreeEntryPtr& A, const FPakTreeEntryPtr& B) -> bool
		{
			if (A->bIsDirectory == B->bIsDirectory)
			{
				return A->Filename.LexicalLess(B->Filename);
			}

			return (int32)A->bIsDirectory > (int32)B->bIsDirectory;
		});
}

void FBaseAnalyzer::RefreshTreeNodeSizePercent(FPakTreeEntryPtr InTreeRoot, FPakTreeEntryPtr InRoot)
{
	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;
		Child->CompressedSizePercentOfTotal = InTreeRoot->CompressedSize > 0 ? (float)Child->CompressedSize / InTreeRoot->CompressedSize : 0.f;
		Child->CompressedSizePercentOfParent = InRoot->CompressedSize > 0 ? (float)Child->CompressedSize / InRoot->CompressedSize : 0.f;

		if (Child->bIsDirectory)
		{
			RefreshTreeNodeSizePercent(InTreeRoot, Child);
		}
	}
}

void FBaseAnalyzer::RetriveFiles(FPakTreeEntryPtr InRoot, const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, const TMap<int32, bool>& InPakIndexFilter, TArray<FPakFileEntryPtr>& OutFiles) const
{
	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;
		if (Child->bIsDirectory)
		{
			RetriveFiles(Child, InFilterText, InClassFilterMap, InPakIndexFilter, OutFiles);
		}
		else
		{
			const bool* bShow = InClassFilterMap.Find(Child->Class);
			const bool* bShowIndex = InPakIndexFilter.Find(Child->OwnerPakIndex);
			const bool bMatchClass = (InClassFilterMap.Num() <= 0 || (bShow && *bShow));
			const bool bMatchIndex = (InPakIndexFilter.Num() <= 0) || (bShowIndex && *bShowIndex);

			if (bMatchClass && bMatchIndex && (InFilterText.IsEmpty() || /*Child->Filename.Contains(InFilterText) ||*/ Child->Path.Contains(InFilterText)))
			{
				OutFiles.Add(Child);
			}
		}
	}
}

void FBaseAnalyzer::RetriveUAssetFiles(FPakTreeEntryPtr InRoot, TArray<FPakFileEntryPtr>& OutFiles) const
{
	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;
		if (Child->bIsDirectory)
		{
			RetriveUAssetFiles(Child, OutFiles);
		}
		else
		{
			if (Child->Filename.ToString().EndsWith(TEXT(".uasset")) || Child->Filename.ToString().EndsWith(TEXT(".umap")))
			{
				OutFiles.Add(Child);
			}
		}
	}
}

void FBaseAnalyzer::InsertClassInfo(FPakTreeEntryPtr InTreeRoot, FPakTreeEntryPtr InRoot, FName InClassName, int32 InFileCount, int64 InSize, int64 InCompressedSize)
{
	FPakClassEntryPtr* ClassEntryPtr = InRoot->FileClassMap.Find(InClassName);
	FPakClassEntryPtr ClassEntry = nullptr;

	if (!ClassEntryPtr)
	{
		ClassEntry = MakeShared<FPakClassEntry>(InClassName, InSize, InCompressedSize, InFileCount);
		InRoot->FileClassMap.Add(InClassName, ClassEntry);
	}
	else
	{
		ClassEntry = *ClassEntryPtr;
		ClassEntry->Class = InClassName;
		ClassEntry->FileCount += InFileCount;
		ClassEntry->Size += InSize;
		ClassEntry->CompressedSize += InCompressedSize;
	}

	ClassEntry->PercentOfTotal = InTreeRoot->CompressedSize > 0 ? (float)ClassEntry->CompressedSize / InTreeRoot->CompressedSize : 0.f;
	ClassEntry->PercentOfParent = InRoot->CompressedSize > 0 ? (float)ClassEntry->CompressedSize / InRoot->CompressedSize : 0.f;
}

FName FBaseAnalyzer::GetAssetClass(const FString& InFilename, FName InPackagePath)
{
	bool bFoundClassInRegistry = false;
	FName AssetClass = *FPaths::GetExtension(InFilename);
	if (AssetRegistryState.IsValid())
	{
		TArrayView<FAssetData const* const> AssetDataArray = AssetRegistryState->GetAssetsByPackageName(InPackagePath);
		if (AssetDataArray.Num() > 0)
		{
			bFoundClassInRegistry = true;
			AssetClass = AssetDataArray[0]->AssetClassPath.GetAssetName();
		}
	}
	
	if (!bFoundClassInRegistry)
	{
		const FName* ClassName = DefaultClassMap.Find(InPackagePath);
		if (ClassName)
		{
			AssetClass = *ClassName;
		}
	}

	return AssetClass.IsNone() ? TEXT("Unknown") : AssetClass;
}

FName FBaseAnalyzer::GetPackagePath(const FString& InFilePath)
{
	FString Left, Right;
	if (InFilePath.Split(TEXT("/Content/"), &Left, &Right))
	{
		const FString Prefix = FPaths::GetPathLeaf(Left);
		const bool bNotUseGamePrefix = Prefix == TEXT("Engine") || InFilePath.Contains(TEXT("Plugin"));

		const FString RemoveFirstExtension = FPaths::SetExtension(Right, TEXT(""));
		const FString RemoveSecondExtension = FPaths::SetExtension(RemoveFirstExtension, TEXT(""));

		return *FString(bNotUseGamePrefix ? TEXT("/") / Prefix / RemoveSecondExtension : TEXT("/Game") / RemoveSecondExtension);
	}
	else
	{
		return *FString(TEXT("/") / FPaths::SetExtension(InFilePath, TEXT("")));
	}
}

void FBaseAnalyzer::Reset()
{
	for (FPakFileSumaryPtr Summary : PakFileSummaries)
	{
		Summary.Reset();
	}

	for (FPakTreeEntryPtr Root : PakTreeRoots)
	{
		Root.Reset();
	}

	PakFileSummaries.Empty();
	PakTreeRoots.Empty();

	AssetRegistryState.Reset();

	AssetRegistryPath = TEXT("");
	DefaultClassMap.Empty();
}

FString FBaseAnalyzer::ResolveCompressionMethod(const FPakFileSumary& Summary, const FPakEntry* InPakEntry) const
{
	if (InPakEntry->CompressionMethodIndex >= 0 && InPakEntry->CompressionMethodIndex < (uint32)Summary.PakInfo.CompressionMethods.Num())
	{
		return Summary.PakInfo.CompressionMethods[InPakEntry->CompressionMethodIndex].ToString();
	}
	else
	{
		return TEXT("Unknown");
	}
}

FPakTreeEntryPtr FBaseAnalyzer::InsertFileToTree(FPakTreeEntryPtr InRoot, const FPakFileSumary& Summary, const FString& InFullPath, const FPakEntry& InPakEntry)
{
	static const TCHAR* Delims[2] = { TEXT("\\"), TEXT("/") };

	TArray<FString> PathItems;
	InFullPath.ParseIntoArray(PathItems, Delims, 2);

	if (PathItems.Num() <= 0)
	{
		return nullptr;
	}

	FString CurrentPath;
	FPakTreeEntryPtr Parent = InRoot;

	for (int32 i = 0; i < PathItems.Num(); ++i)
	{
		CurrentPath = CurrentPath / PathItems[i];

		FPakTreeEntryPtr* Child = Parent->ChildrenMap.Find(*PathItems[i]);
		if (Child)
		{
			Parent = *Child;
			continue;
		}
		else
		{
			const bool bLastItem = (i == PathItems.Num() - 1);

			FPakTreeEntryPtr NewChild = MakeShared<FPakTreeEntry>(*PathItems[i], CurrentPath, !bLastItem);
			if (bLastItem)
			{
				NewChild->PakEntry = InPakEntry;
				NewChild->CompressionMethod = *ResolveCompressionMethod(Summary, &InPakEntry);
				NewChild->PackagePath = GetPackagePath(CurrentPath);
			}

			Parent->ChildrenMap.Add(*PathItems[i], NewChild);
			Parent = NewChild;
		}
	}

	return Parent;
}
