# UnrealPakViewer ##

Allows the user to view the contents of .pak files. Currently supports the following features:

* Tree view and list view to view files in the pak/ucas
* Open multiple pak/ucas files at the same time
* Search, filter, sort in list view
* View the specific content and composition information of the UAsset file
* Display the size of each folder, file, and file type as a percentage
* Multi-threaded decompression of Pak files
* Loading the AssetRegistry.bin resource registry

## Function ##

### Opening the .pak file ###

![OpenPak.png](Resources/Images/OpenPak.png)

Directly drag the Pak file to the UnrealPakViewer window to open it, if the Pak file is encrypted, a password input box will pop up

![AESKey.png](Resources/Images/AESKey.png)

After entering the Base64 format of the corresponding AES key, you can open the Pak file

### Viewing Pak file summary information ###

![PakSummary.png](Resources/Images/PakSummary.png)

### Loading the resource registry ###

![LoadAssetRegistry.png](Resources/Images/LoadAssetRegistry.png)

After cooking completes, it will dump resource registry information in *Saved/Cooked/[Platform]/[Project]/Metadata/DevelopmentAssetRegistry.bin*, which contains resource type, reference relationship and other information.

It can be loaded through Load Asset Registry to analyze the size ratio information of each resource type

### File tree view ###

![TreeView.png](Resources/Images/TreeView.png)

Lists the directories and files contained in the Pak in a tree structure, as well as the ratio of the directory size to the total size

#### After selecting a directory, you can view the details of the directory on the right ####

![FolderDetail.png](Resources/Images/FolderDetail.png)

And the proportion information of each file type in the directory (you will need to load the AssetRegistry.bin registry)

![FolderDetailClass.png](Resources/Images/FolderDetailClass.png)

#### After selecting the file, you can view the file details on the right ####

![FileDetail.png](Resources/Images/FileDetail.png)

Compared to the contents, there is more information

If you select a .uasset or .umap file, you can also view the serialization information inside the file

![AssetSummary.png](Resources/Images/AssetSummary.png)

![ImportObjects.png](Resources/Images/ImportObjects.png)

![ExportObjects.png](Resources/Images/ExportObjects.png)

![ObjectDependencies.png](Resources/Images/ObjectDependencies.png)

![DependencyPackages.png](Resources/Images/DependencyPackages.png)

![DependentPackages.png](Resources/Images/DependentPackages.png)

![Names.png](Resources/Images/Names.png)

#### Right-click menu ####

![TreeViewContext.png](Resources/Images/TreeViewContext.png)

### File list view ###

![ListView.png](Resources/Images/ListView.png)

The file list view displays the file information in the Pak in a table format, and supports sorting by clicking on the column headings

#### Type filtering ####

![ClassFilter.png](Resources/Images/ClassFilter.png)

Filter files in the list by type

#### File name filtering ####

![NameFilter.png](Resources/Images/NameFilter.png)

Filter the files in the list by file name

#### Right-click menu ####

![ListViewContext.png](Resources/Images/ListViewContext.png)

## Compiling ##

Clone the code to the *Engine\Source\Programs* directory, open the solution and compile it

* The versions of the engine that have been compiled
  * 4.24
  * 4.25
  * 4.26
  * 4.27
  * 4.28

## TODO ##

* commandline application
* Pak compare visiualize
* resource preview
* resource load heat map
