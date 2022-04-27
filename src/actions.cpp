#include <string.h>
#include "fs.h"
#include "config.h"
#include "windows.h"
#include "ftpclient.h"
#include "util.h"

namespace Actions {
    
    void RefreshLocalFiles()
    {
        multi_selected_local_files.clear();
        local_files.clear();
        int err;
        if (strlen(local_filter)>0)
        {
            std::vector<FsEntry> temp_files = FS::ListDir(local_directory, &err);
            std::string lower_filter = Util::ToLower(local_filter);
            for (std::vector<FsEntry>::iterator it=temp_files.begin(); it!=temp_files.end(); )
            {
                std::string lower_name = Util::ToLower(it->name);
                if (lower_name.find(lower_filter) != std::string::npos || strcmp(it->name, "..") == 0)
                {
                    local_files.push_back(*it);
                }
                ++it;
            }
            temp_files.clear();
        }
        else
        {
            local_files = FS::ListDir(local_directory, &err);
        }
        FS::Sort(local_files);
        if (err != 0)
            sprintf(status_message, "Failed to read contents of directory \"%s\" or folder does not exists.", local_directory);
    }

    void RefreshRemoteFiles()
    {
        if (!ftpclient->IsConnected())
        {
            return;
        }

        multi_selected_remote_files.clear();
        remote_files.clear();
        if (strlen(remote_filter)>0)
        {
            std::vector<FsEntry> temp_files = ftpclient->ListDir(remote_directory);
            std::string lower_filter = Util::ToLower(remote_filter);
            for (std::vector<FsEntry>::iterator it=temp_files.begin(); it!=temp_files.end(); )
            {
                std::string lower_name = Util::ToLower(it->name);
                if (lower_name.find(lower_filter) != std::string::npos || strcmp(it->name, "..") == 0)
                {
                    remote_files.push_back(*it);
                }
                ++it;
            }
            temp_files.clear();
        }
        else
        {
            remote_files = ftpclient->ListDir(remote_directory);
        }
        FS::Sort(remote_files);
        sprintf(status_message, "%s", ftpclient->LastResponse());
    }

    void HandleChangeLocalDirectory(FsEntry *entry)
    {
        if (!entry->isDir)
            return;

        if (strcmp(entry->name, "..") == 0)
        {
            std::string temp_path = std::string(entry->directory);
            sprintf(local_directory, "%s", temp_path.substr(0, temp_path.find_last_of("/")).c_str());
            sprintf(local_file_to_select, "%s", temp_path.substr(temp_path.find_last_of("/")+1).c_str());
        }
        else
        {
            sprintf(local_directory, "%s", entry->path);
        }
        RefreshLocalFiles();
        if (strcmp(entry->name, "..") != 0)
        {
            sprintf(local_file_to_select, "%s", local_files[0].name);
        }
        selected_local_file = nullptr;
        selected_action = ACTION_NONE;
    }

    void HandleChangeRemoteDirectory(FsEntry *entry)
    {
        if (!entry->isDir)
            return;
            
        if (strcmp(entry->name, "..") == 0)
        {
            std::string temp_path = std::string(entry->directory);
            if (temp_path.size()>1)
            {
                if (temp_path.find_last_of("/") == 0)
                {
                    sprintf(remote_directory, "/");
                }
                else
                {
                    sprintf(remote_directory, "%s", temp_path.substr(0, temp_path.find_last_of("/")).c_str());
                }
            }
            sprintf(remote_file_to_select, "%s", temp_path.substr(temp_path.find_last_of("/")+1).c_str());
        }
        else
        {
            sprintf(remote_directory, "%s", entry->path);
        }
        RefreshRemoteFiles();
        if (strcmp(entry->name, "..") != 0)
        {
            sprintf(remote_file_to_select, "%s", remote_files[0].name);
        }
        selected_remote_file = nullptr;
        selected_action = ACTION_NONE;
    }

    void HandleRefreshLocalFiles()
    {
        int prev_count = local_files.size();
        RefreshLocalFiles();
        int new_count = local_files.size();
        if (prev_count != new_count)
        {
            sprintf(local_file_to_select, "%s", local_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleRefreshRemoteFiles()
    {
        int prev_count = remote_files.size();
        RefreshRemoteFiles();
        int new_count = remote_files.size();
        if (prev_count != new_count)
        {
            sprintf(remote_file_to_select, "%s", remote_files[0].name);
        }
        selected_action = ACTION_NONE;
    }

    void HandleClearLocalFilter()
    {
        sprintf(local_filter, "");
        HandleRefreshLocalFiles();
    }

    void HandleClearRemoteFilter()
    {
        sprintf(remote_filter, "");
        HandleRefreshRemoteFiles();
    }

    void CreateNewLocalFolder(char *new_folder)
    {
        std::string folder = std::string(new_folder);
        folder = Util::Rtrim(Util::Trim(folder, " "), "/");
        std::string path = FS::GetPath(local_directory, folder);
        FS::MkDirs(path);
        RefreshLocalFiles();
        sprintf(local_file_to_select, "%s", folder.c_str());
    }

    void CreateNewRemoteFolder(char *new_folder)
    {
        std::string folder = std::string(new_folder);
        folder = Util::Rtrim(Util::Trim(folder, " "), "/");
        std::string path = FS::GetPath(remote_directory, folder);
        ftpclient->Mkdir(path.c_str());
        RefreshRemoteFiles();
        sprintf(remote_file_to_select, "%s", folder.c_str());
    }

    void RenameLocalFolder(char *old_path, char *new_path)
    {
        std::string new_name = std::string(new_path);
        new_name = Util::Rtrim(Util::Trim(new_name, " "), "/");
        std::string path = FS::GetPath(local_directory, new_name);
        FS::Rename(old_path, path);
        RefreshLocalFiles();
        sprintf(local_file_to_select, "%s", new_name.c_str());
    }

    void RenameRemoteFolder(char *old_path, char *new_path)
    {
        std::string new_name = std::string(new_path);
        new_name = Util::Rtrim(Util::Trim(new_name, " "), "/");
        std::string path = FS::GetPath(remote_directory, new_name);
        ftpclient->Rename(old_path, path.c_str());
        RefreshRemoteFiles();
        sprintf(remote_file_to_select, "%s", new_name.c_str());
    }

    int DeleteSelectedLocalFilesThread(SceSize args, void *argp)
    {
        for (std::set<FsEntry>::iterator it = multi_selected_local_files.begin(); it != multi_selected_local_files.end(); ++it)
        {
            FS::RmRecursive(it->path);
        }
        activity_inprogess = false;
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return sceKernelExitDeleteThread(0);
    }

    void DeleteSelectedLocalFiles()
    {
        bk_activity_thid = sceKernelCreateThread("delete_files_thread", (SceKernelThreadEntry)DeleteSelectedLocalFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
		if (bk_activity_thid >= 0)
			sceKernelStartThread(bk_activity_thid, 0, NULL);
    }

    int DeleteSelectedRemotesFilesThread(SceSize args, void *argp)
    {
        for (std::set<FsEntry>::iterator it = multi_selected_remote_files.begin(); it != multi_selected_remote_files.end(); ++it)
        {
            if (it->isDir)
                ftpclient->Rmdir(it->path, true);
            else
                ftpclient->Delete(it->path);
        }
        activity_inprogess = false;
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return sceKernelExitDeleteThread(0);
    }

    void DeleteSelectedRemotesFiles()
    {
        bk_activity_thid = sceKernelCreateThread("delete_files_thread", (SceKernelThreadEntry)DeleteSelectedRemotesFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
		if (bk_activity_thid >= 0)
			sceKernelStartThread(bk_activity_thid, 0, NULL);
    }

    int Upload(const FsEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<FsEntry> entries = FS::ListDir(src.path, &err);
            ftpclient->Mkdir(dest);
            for (int i=0; i<entries.size(); i++)
            {
                if (stop_activity)
		            return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    ftpclient->Mkdir(new_path);
                    ret = Upload(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "Uploading %s", entries[i].path);
                    ret = ftpclient->Put(entries[i].path, new_path, FtpClient::transfermode::image, 0);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "Failed to upload file %s", entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "Uploading %s", src.name);
            ret = ftpclient->Put(src.path, new_path, FtpClient::transfermode::image, 0);
            if (ret <= 0)
            {
                free(new_path);
                sprintf(status_message, "Failed to upload file %s", src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    int UploadFilesThread(SceSize args, void *argp)
    {
        for (std::set<FsEntry>::iterator it = multi_selected_local_files.begin(); it != multi_selected_local_files.end(); ++it)
        {
            if (it->isDir)
            {
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", remote_directory, FS::hasEndSlash(remote_directory)? "" : "/", it->name);
                Upload(*it, new_dir);
            }
            else
            {
                Upload(*it, remote_directory);
            }
        }
        activity_inprogess = false;
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_REMOTE_FILES;
        return sceKernelExitDeleteThread(0);
    }
    
    void UploadFiles()
    {
        bk_activity_thid = sceKernelCreateThread("upload_files_thread", (SceKernelThreadEntry)UploadFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
		if (bk_activity_thid >= 0)
			sceKernelStartThread(bk_activity_thid, 0, NULL);
    }

    int Download(const FsEntry &src, const char *dest)
    {
        if (stop_activity)
            return 1;

        int ret;
        if (src.isDir)
        {
            int err;
            std::vector<FsEntry> entries = ftpclient->ListDir(src.path);
            FS::MkDirs(dest);
            for (int i=0; i<entries.size(); i++)
            {
                if (stop_activity)
		            return 1;

                int path_length = strlen(dest) + strlen(entries[i].name) + 2;
                char *new_path = malloc(path_length);
                snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", entries[i].name);

                if (entries[i].isDir)
                {
                    if (strcmp(entries[i].name, "..") == 0)
                        continue;

                    FS::MkDirs(new_path);
                    ret = Download(entries[i], new_path);
                    if (ret <= 0)
                    {
                        free(new_path);
                        return ret;
                    }
                }
                else
                {
                    snprintf(activity_message, 1024, "Downloading %s", entries[i].path);
                    ret = ftpclient->Get(new_path, entries[i].path, FtpClient::transfermode::image, 0);
                    if (ret <= 0)
                    {
                        sprintf(status_message, "Failed to downloadload file %s", entries[i].path);
                        free(new_path);
                        return ret;
                    }
                }
                free(new_path);
            }
        }
        else
        {
            int path_length = strlen(dest) + strlen(src.name) + 2;
            char *new_path = malloc(path_length);
            snprintf(new_path, path_length, "%s%s%s", dest, FS::hasEndSlash(dest) ? "" : "/", src.name);
            snprintf(activity_message, 1024, "Downloading %s", src.name);
            ret = ftpclient->Get(new_path, src.path, FtpClient::transfermode::image, 0);
            if (ret <= 0)
            {
                free(new_path);
                sprintf(status_message, "Failed to download file %s", src.name);
                return 0;
            }
            free(new_path);
        }
        return 1;
    }

    int DownloadFilesThread(SceSize args, void *argp)
    {
        for (std::set<FsEntry>::iterator it = multi_selected_remote_files.begin(); it != multi_selected_remote_files.end(); ++it)
        {
            if (it->isDir)
            {
                char new_dir[512];
                sprintf(new_dir, "%s%s%s", local_directory, FS::hasEndSlash(local_directory)? "" : "/", it->name);
                Download(*it, new_dir);
            }
            else
            {
                Download(*it, local_directory);
            }
        }
        activity_inprogess = false;
        Windows::SetModalMode(false);
        selected_action = ACTION_REFRESH_LOCAL_FILES;
        return sceKernelExitDeleteThread(0);
    }
    
    void DownloadFiles()
    {
        bk_activity_thid = sceKernelCreateThread("download_files_thread", (SceKernelThreadEntry)DownloadFilesThread, 0x10000100, 0x4000, 0, 0, NULL);
		if (bk_activity_thid >= 0)
			sceKernelStartThread(bk_activity_thid, 0, NULL);
    }

    void ConnectFTP()
    {
        CONFIG::SaveConfig();
        if (ftpclient->Connect(ftp_settings.server_ip, ftp_settings.server_port))
        {
            if (ftpclient->Login(ftp_settings.username, ftp_settings.password))
            {
                RefreshRemoteFiles();
                sprintf(status_message, "%s", ftpclient->LastResponse());
            }
            else
            {
                sprintf(status_message, "300 Failed Login. Please check your username or password.");
            }
        }
        else
        {
            sprintf(status_message, "300 Failed. Connection timeout.");
        }
        selected_action = ACTION_NONE;
    }
}
