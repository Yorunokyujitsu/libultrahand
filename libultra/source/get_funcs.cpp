/********************************************************************************
 * File: get_funcs.cpp
 * Author: ppkantorski
 * Description:
 *   This source file provides the implementations of functions declared in
 *   get_funcs.hpp. These functions are responsible for retrieving and handling
 *   data from the file system and JSON files, including parsing overlay module
 *   information, reading file contents, and accessing structured data used
 *   in the Ultrahand Overlay project.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository.
 *   (GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay)
 *
 *   Note: Please be aware that this notice cannot be altered or removed. It is a part
 *   of the project's documentation and must remain intact.
 * 
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2024 ppkantorski
 ********************************************************************************/

#include "get_funcs.hpp"


namespace ult {
    
    /**
     * @brief Reads the contents of a file and returns it as a string, normalizing line endings.
     *
     * @param filePath The path to the file to be read.
     * @return The content of the file as a string with line endings normalized to '\n'.
     */
    //std::string getFileContents(const std::string& filePath) {
    //    #if !USING_FSTREAM_DIRECTIVE
    //    FILE* file = fopen(filePath.c_str(), "rb");
    //    if (!file) {
    //        #if USING_LOGGING_DIRECTIVE
    //        logMessage("Failed to open file: " + filePath);
    //        #endif
    //        return "";
    //    }
    //
    //    // Determine the file size
    //    fseek(file, 0, SEEK_END);
    //    long size = ftell(file);
    //    if (size <= 0) {
    //        fclose(file);
    //        return "";
    //    }
    //    fseek(file, 0, SEEK_SET);
    //
    //    // Read the entire file into a string
    //    std::string content(size, '\0');
    //    if (fread(&content[0], 1, size, file) != static_cast<size_t>(size)) {
    //        #if USING_LOGGING_DIRECTIVE
    //        logMessage("Failed to read file: " + filePath);
    //        #endif
    //        fclose(file);
    //        return "";
    //    }
    //
    //    fclose(file);
    //
    //    #else
    //    std::ifstream file(filePath, std::ios::binary);
    //    if (!file) {
    //        #if USING_LOGGING_DIRECTIVE
    //        logMessage("Failed to open file: " + filePath);
    //        #endif
    //        return "";
    //    }
    //
    //    // Determine the file size
    //    file.seekg(0, std::ios::end);
    //    std::streamsize size = file.tellg();
    //    if (size <= 0) {
    //        return "";
    //    }
    //    file.seekg(0, std::ios::beg);
    //
    //    // Read the entire file into a string
    //    std::string content(size, '\0');
    //    if (!file.read(&content[0], size)) {
    //        #if USING_LOGGING_DIRECTIVE
    //        logMessage("Failed to read file: " + filePath);
    //        #endif
    //        return "";
    //    }
    //    #endif
    //
    //    // Erase any carriage return characters (normalize line endings)
    //    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
    //    return content;
    //}

    
    
    
    /**
     * @brief Concatenates the provided directory and file names to form a destination path.
     *
     * @param destinationDir The directory where the file should be placed.
     * @param fileName The name of the file.
     * @return The destination path as a string.
     */
    std::string getDestinationPath(const std::string& destinationDir,
                                   const std::string& fileName)
    {
        // e.g. "foo/bar" + "/" + "baz.txt"  → "foo/bar/baz.txt", but if destinationDir ended in '/',
        // you’d get "foo/bar//baz.txt" → collapse again:
        std::string combined = destinationDir + "/" + fileName;
        preprocessPath(combined);
        return combined;
    }
    
    /**
     * @brief Extracts the value part from a string line containing a key-value pair.
     *
     * @param line The string line containing a key-value pair (e.g., "key=value").
     * @return The extracted value as a string. If no value is found, an empty string is returned.
     */
    std::string getValueFromLine(const std::string& line) {
        // Find the position of '=' character from the end of the string
        size_t equalsPos = line.rfind('=');
        if (equalsPos != std::string::npos && equalsPos + 1 < line.size()) {
            // Directly return the trimmed substring after '='
            std::string newLine = line.substr(equalsPos + 1);
            trim(newLine);
            return newLine;
        }
        return ""; // Return an empty string if '=' is not found or no content after '='
    }
    
    
    
    /**
     * @brief Extracts the name from a file path, including handling directories.
     *
     * @param path The file path from which to extract the name.
     * @return The extracted name as a string. If the path indicates a directory, it extracts the last directory name.
     * If the path is empty or no name is found, an empty string is returned.
     */
    std::string getNameFromPath(const std::string& path) {
        size_t lastNonSlash = path.find_last_not_of('/');
        if (lastNonSlash == std::string::npos) {
            return "";  // All slashes, or empty string effectively
        }
    
        size_t lastSlash = path.find_last_of('/', lastNonSlash);
        if (lastSlash == std::string::npos) {
            return path.substr(0, lastNonSlash + 1);  // No slashes, the entire path is a filename
        }
    
        return path.substr(lastSlash + 1, lastNonSlash - lastSlash);  // Standard case, efficiently handled
    }
    
    
    
    /**
     * @brief Extracts the file name from a full file path.
     *
     * This function takes a filesystem path and returns only the file name,
     * stripping away any directory paths that precede it.
     *
     * @param path The full path to the file.
     * @return The file name extracted from the full path.
     */
    std::string getFileName(const std::string& path) {
        // Find the last slash in the path
        size_t pos = path.find_last_of("/");
        if (pos != std::string::npos) {
            // Return the substring after the last slash
            return path.substr(pos + 1);
        }
        return "";
    }
    
    
    
    /**
     * @brief Extracts the name of the parent directory from a given file path at a specified level.
     *
     * @param path The file path from which to extract the parent directory name.
     * @param level The level of the parent directory to extract (0 for immediate parent, 1 for grandparent, and so on).
     * @return The name of the parent directory at the specified level.
     */
    std::string getParentDirNameFromPath(const std::string& path, size_t level) {
        if (path.empty()) return "";
    
        // Start from the end of the string and move backwards to find the slashes
        size_t endPos = path.find_last_not_of('/');
        if (endPos == std::string::npos) return ""; // All slashes or empty path
    
        size_t pos = path.rfind('/', endPos);
        if (pos == std::string::npos || pos == 0) return ""; // No parent directory or single slash
    
        while (level-- > 0 && pos != std::string::npos) {
            endPos = pos - 1;
            pos = path.rfind('/', endPos);
            if (pos == std::string::npos || pos == 0) return ""; // No more directories to go up
        }
    
        size_t start = path.rfind('/', pos - 1);
        if (start == std::string::npos) start = 0;
        else start += 1; // Move past the slash
    
        std::string parentDir = path.substr(start, pos - start);
    
        // Check for spaces or special characters
        if (parentDir.find_first_of(" \t\n\r\f\v") != std::string::npos) {
            // If it does, return the directory name within quotes
            return "\"" + parentDir + "\"";
        }
    
        return parentDir;
    }
    
    
    
    /**
     * @brief Extracts the parent directory path from a given file path.
     *
     * @param path The file path from which to extract the parent directory path.
     * @return The parent directory path.
     */
    std::string getParentDirFromPath(const std::string& path) {
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash != std::string::npos) {
            return  path.substr(0, lastSlash + 1);
        }
        return path;
    }
    
    
    
    /**
     * @brief Gets a list of subdirectories in a directory.
     *
     * @param directoryPath The path of the directory to search.
     * @return A vector of strings containing the names of subdirectories.
     */
    std::vector<std::string> getSubdirectories(const std::string& directoryPath) {
        std::vector<std::string> subdirectories;
        std::unique_ptr<DIR, DirCloser> dir(opendir(directoryPath.c_str()));
        
        if (!dir) return subdirectories;
        
        struct dirent* entry;
        while ((entry = readdir(dir.get())) != nullptr) {
            const std::string entryName = entry->d_name;
            
            // Skip . and ..
            if (entryName == "." || entryName == "..") continue;
            
            const std::string fullPath = directoryPath + "/" + entryName;
            
            if (isDirectory(entry, fullPath)) {
                subdirectories.emplace_back(entryName);
            }
        }
        
        return subdirectories;
    }
    
    /**
     * @brief Check if a directory entry is a directory (no caching).
     * Fast path for known types, stat() only when necessary.
     */
    inline bool isDirectory(struct dirent* entry, const std::string& path) {
        // Fast path - most filesystems populate d_type correctly
        if (entry->d_type == DT_DIR) {
            return true;
        } else if (entry->d_type != DT_UNKNOWN) {
            return false;  // DT_REG, DT_LNK, etc.
        }
        
        // Only stat when d_type is unknown (rare on modern filesystems)
        struct stat entryStat;
        return (stat(path.c_str(), &entryStat) == 0) && S_ISDIR(entryStat.st_mode);
    }
    
    
    /**
     * @brief Recursively retrieves a list of files from a directory.
     *
     * @param directoryPath The path of the directory to search.
     * @return A vector of strings containing the paths of the files.
     */
    std::vector<std::string> getFilesListFromDirectory(const std::string& directoryPath) {
        std::vector<std::string> fileList;
        std::unique_ptr<DIR, DirCloser> dir(opendir(directoryPath.c_str()));

        if (!dir) return fileList;

        struct dirent* entry;
        while ((entry = readdir(dir.get())) != nullptr) {
            const std::string entryName = entry->d_name;
            
            if (entryName == "." || entryName == "..") continue;
            
            const std::string fullPath = directoryPath + "/" + entryName;
            
            if (entry->d_type == DT_REG) {
                // Definitely a regular file
                fileList.emplace_back(fullPath);
            } else if (isDirectory(entry, fullPath)) {
                // Recursively get files from subdirectories
                auto subDirFiles = getFilesListFromDirectory(fullPath);
                //fileList.reserve(fileList.size() + subDirFiles.size());
                fileList.insert(fileList.end(), 
                              std::make_move_iterator(subDirFiles.begin()),
                              std::make_move_iterator(subDirFiles.end()));
            }
        }

        return fileList;
    }
    
    // Helper function to check if a path is a directory
    //bool isDirectoryCached(const struct dirent* entry, const std::string& fullPath) {
    //    struct stat st;
    //    if (stat(fullPath.c_str(), &st) != 0) return false;
    //    return S_ISDIR(st.st_mode);
    //}
    
    // Iterative function to handle wildcard directories and file patterns
    void handleDirectory(const std::string& basePath, 
                        const std::vector<std::string>& parts, 
                        size_t partIndex, 
                        std::vector<std::string>& results, 
                        bool directoryOnly,
                        size_t maxLines) {
        
        std::vector<std::pair<std::string, size_t>> stack;
        stack.emplace_back(basePath, partIndex);
        
        std::string fullPath;
        std::string result;
        struct stat st;
        
        bool isDir;
        while (!stack.empty()) {
            if (maxLines > 0 && results.size() >= maxLines) return;
            
            auto [currentPath, currentPartIndex] = stack.back();
            stack.pop_back();
            
            if (currentPartIndex >= parts.size()) continue;
    
            DIR* dirPtr = opendir(currentPath.c_str());
            if (!dirPtr) continue;
            std::unique_ptr<DIR, DirCloser> dir(dirPtr);
    
            const std::string& pattern = parts[currentPartIndex];
            const bool isLastPart = (currentPartIndex == parts.size() - 1);
            const bool needsSlash = currentPath.back() != '/';
    
            struct dirent* entry;
            while ((entry = readdir(dir.get())) != nullptr) {
                if (maxLines > 0 && results.size() >= maxLines) return;
                
                const char* name = entry->d_name;
                if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;
                
                if (fnmatch(pattern.c_str(), name, FNM_NOESCAPE) != 0) continue;
                
                
                if (entry->d_type != DT_UNKNOWN) {
                    isDir = (entry->d_type == DT_DIR);
                } else {
                    fullPath = currentPath + (needsSlash ? "/" : "") + name;
                    isDir = (stat(fullPath.c_str(), &st) == 0) && S_ISDIR(st.st_mode);
                }
                
                if (isLastPart) {
                    if (!directoryOnly || isDir) {
                        result = currentPath + (needsSlash ? "/" : "") + name;
                        if (isDir) result += '/';
                        results.emplace_back(std::move(result));
                        if (maxLines > 0 && results.size() >= maxLines) return;
                    }
                } else if (isDir) {
                    stack.emplace_back(currentPath + (needsSlash ? "/" : "") + name, currentPartIndex + 1);
                }
            }
        }
    }
    
    /**
     * @brief Gets a list of files and folders based on a wildcard pattern.
     *
     * This function searches for files and folders in a directory that match the
     * specified wildcard pattern.
     *
     * @param pathPattern The wildcard pattern to match files and folders.
     * @return A vector of strings containing the paths of matching files and folders.
     */
    std::vector<std::string> getFilesListByWildcards(const std::string& pathPattern, size_t maxLines) {
        std::vector<std::string> results;
        
        if (pathPattern.empty()) return results;
        
        const bool directoryOnly = pathPattern.back() == '/';
        const size_t prefixEnd = pathPattern.find(":/");
        
        if (prefixEnd == std::string::npos) return results;
        
        const std::string basePath = pathPattern.substr(0, prefixEnd + 2);
        std::vector<std::string> parts;
        
        size_t start = prefixEnd + 2;
        size_t pos;
        while ((pos = pathPattern.find('/', start)) != std::string::npos) {
            if (pos > start) {
                parts.emplace_back(pathPattern.substr(start, pos - start));
            }
            start = pos + 1;
        }
        
        if (start < pathPattern.length() && !directoryOnly) {
            parts.emplace_back(pathPattern.substr(start));
        }
        
        if (!parts.empty()) {
            handleDirectory(basePath, parts, 0, results, directoryOnly, maxLines);
        }
        
        return results;
    }
    
}