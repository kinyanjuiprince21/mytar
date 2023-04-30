#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
#include <sys/time.h>

#include "inodemap.h"

#define MAGIC_CONSTANT 0x1A6F347D
int firstFile = 1;

int create_tar_file(const char *dirname, const char *filename)
{
    struct stat buf;
    struct stat temp_buf;
    DIR *d;
    struct dirent *de;
    int exists;
    char *fullname;
    FILE *file;
    uint32_t magic_number;
    uint64_t inode;
    uint32_t name_length;
    uint32_t file_mode;
    uint64_t file_modification_time;
    uint64_t file_size;
    char *content;
    if (lstat(dirname, &temp_buf) < 0)
    {
        fprintf(stderr, "Error: Specified target (\"%s\") does not exist.\n", dirname);
        return 0;
    }
    if (firstFile)
    {
        file = fopen(filename, "wb");
        if (file == NULL)
        {
            perror("Error: No tarfile specified.\n");
            return 0;
        }

        // write magic number to the beginning of our archive file, this will determine the format of our archive file
        magic_number = MAGIC_CONSTANT;
        fwrite(&magic_number, 1, sizeof(magic_number), file);
        if (fclose(file) == EOF)
        {
            return 0;
        }
        firstFile = 0;
    }
    d = opendir(dirname);
    if (d == NULL)
    {
        perror("Error: ");
        return 0;
    }
    for (de = readdir(d); de != NULL; de = readdir(d))
    {
        if (strlen(de->d_name) == strlen("..") && strcmp(de->d_name, "..") == 0)
        {
            continue;
        }
        fullname = (char *)malloc(sizeof(char) * (strlen(dirname) + 258));
        if (strlen(de->d_name) == strlen(".") && strcmp(de->d_name, ".") == 0)
        {
            sprintf(fullname, "%s", dirname);
        }
        else
        {
            sprintf(fullname, "%s/%s", dirname, de->d_name);
        }
        exists = lstat(fullname, &buf);
        if (exists < 0)
        {
            perror("Error: ");
            free(fullname);
            fullname = NULL;
            continue;
        }
        if (S_ISLNK(buf.st_mode))
        {
            free(fullname);
            fullname = NULL;
            continue;
        }
        inode = (uint64_t)buf.st_ino;
        name_length = (uint32_t)strlen(fullname);
        if (get_inode(buf.st_ino))
        {
            if (S_ISDIR(buf.st_mode))
            {
                free(fullname);
                fullname = NULL;
                continue;
            }
            // write hard link
            FILE *file = fopen(filename, "a");
            if (file != NULL)
            {
                fwrite(&inode, 1, sizeof(inode), file);
                fwrite(&name_length, 1, sizeof(name_length), file);
                fwrite(fullname, 1, name_length, file);
                fclose(file);
            }
            free(fullname);
            fullname = NULL;
            continue;
        }
        set_inode(buf.st_ino, filename);
        file_mode = (uint32_t)buf.st_mode;
        file_modification_time = (uint64_t)buf.st_mtime;
        if (S_ISDIR(buf.st_mode))
        {

            // write directory
            FILE *file;
            file = fopen(filename, "a");
            if (file != NULL)
            {
                fwrite(&inode, 1, sizeof(inode), file);
                fwrite(&name_length, 1, sizeof(name_length), file);
                fwrite(fullname, 1, name_length, file);
                fwrite(&file_mode, 1, sizeof(file_mode), file);
                fwrite(&file_modification_time, 1, sizeof(file_modification_time), file);
                fclose(file);

                if (strlen(fullname) != strlen(dirname) || strcmp(fullname, dirname) != 0)
                {
                    create_tar_file(fullname, filename);
                }
                free(fullname);
                fullname = NULL;
            }

            continue;
        }
        if (strlen(filename) == strlen(de->d_name) && strcmp(filename, de->d_name) == 0)
        {
            file_size = 0;
        }
        else
        {
            file_size = (uint64_t)buf.st_size;
        }
        if (buf.st_mode)
        {
            FILE *input;

            FILE *file;
            file = fopen(filename, "a");
            if (file != NULL)
            {
                fwrite(&inode, 1, sizeof(inode), file);
                fwrite(&name_length, 1, sizeof(name_length), file);
                fwrite(fullname, 1, name_length, file);
                fwrite(&file_mode, 1, sizeof(file_mode), file);
                fwrite(&file_modification_time, 1, sizeof(file_modification_time), file);
                fwrite(&file_size, 1, sizeof(file_size), file);
                if (file_size != 0)
                {
                    content = (char *)malloc(sizeof(char) * file_size);
                    fread(content, 1, sizeof(char) * file_size, file);
                    fwrite(content, 1, file_size, file);
                    free(content);
                }
                fclose(file);
            }
            else
            {
                perror("Error: ");
                free(fullname);
                fullname = NULL;
                continue;
            }

            free(fullname);
            fullname = NULL;
            continue;
        }
        perror("Error: ");
    }
    closedir(d);
}

int file_exists(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "Unable to open file '%s'\n", filename);
        return 1;
    }

    uint64_t inode;
    uint32_t name_length;
    char *file_name;
    uint32_t file_mode;
    uint64_t file_size;
    struct stat buf;

    while (fread(&inode, sizeof(inode), 1, file) == 1)
    {
        if (fread(&name_length, 1, sizeof(name_length), file) != sizeof(name_length))
        {
            break;
        }

        file_name = (char *)malloc(name_length + 1);
        if (file_name == NULL)
        {
            fprintf(stderr, "Error: memory allocation failed\n");
            fclose(file);
            return 0;
        }

        fread(file_name, name_length, 1, file);
        file_name[name_length] = '\0';

        if (lstat(file_name, &buf) == 0)
        {
            fprintf(stderr, "File '%s' exists in dir.\n", file_name);
            free(file_name);
            fclose(file);
            return 0;
        }

        set_inode(inode, file_name);

        fread(&file_mode, sizeof(file_mode), 1, file);
        fseek(file, sizeof(uint64_t), SEEK_CUR);

        if (S_ISDIR(file_mode))
        {
            free(file_name);
            continue;
        }

        fread(&file_size, sizeof(file_size), 1, file);
        fseek(file, file_size, SEEK_CUR);

        free(file_name);
    }

    fclose(file);
    return 1;
}

int extract_tar(const char *filename)
{
    FILE *file, *output;
    uint32_t magic_number, name_length, file_mode;
    char *file_name, *hard_link;
    uint64_t file_modification_time, file_size, inode;
    char *content;
    struct stat buf;
    struct timeval mtime, mytime, filetimes[2];

    if (!file_exists(filename))
    {
        return 0;
    }
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        return 0;
    }
    if (fread(&magic_number, 1, sizeof(magic_number), file) != sizeof(magic_number))
    {
        if (fclose(file) == EOF)
        {
            return 0;
        }
        free_inode();
        return 1;
    }
    if (magic_number != MAGIC_CONSTANT)
    {
        fprintf(stderr, "Error: Invalid archive file\n");
        if (fclose(file) == EOF)
        {
            return 0;
        }
        free_inode();
        return 1;
    }
    while (!feof(file))
    {
        if (fread(&inode, 1, sizeof(inode), file) != sizeof(inode))
        {
            break;
        }
        if (fread(&name_length, 1, sizeof(name_length), file) != sizeof(name_length))
        {
            break;
        }
        file_name = (char *)malloc(name_length + 1);
        if (fread(file_name, 1, name_length, file) != name_length)
        {
            free(file_name);
            break;
        }
        file_name[name_length] = '\0';
        if ((hard_link = get_inode(inode)) != NULL)
        {
            FILE *input;
            if (lstat(hard_link, &buf) != -1)
            {
                input = fopen(hard_link, "rb");
                if (input == NULL)
                {
                    free(file_name);
                    perror("Error: ");
                    if (fclose(file) == EOF)
                    {
                        return 0;
                    }
                    free_inode();
                    return 1;
                }
                file_size = (uint64_t)buf.st_size;
                content = (char *)malloc(sizeof(char) * file_size);
                fread(content, 1, file_size, input);
                output = fopen(file_name, "wb");
                fwrite(content, 1, file_size, output);
                fclose(output);
                free(content);
                if (fclose(input) == EOF)
                {
                    perror("Error: ");
                    if (fclose(file) == EOF)
                    {
                        return 0;
                    }
                    free_inode();
                    return 1;
                }
            }
            if (fclose(file) == EOF)
            {
                return 0;
            }
            free_inode();
            return 1;
        }
        set_inode(inode, file_name);
        if (fread(&file_mode, 1, sizeof(file_mode), file) != sizeof(file_mode))
        {
            break;
        }
        if (fread(&file_modification_time, 1, sizeof(file_modification_time), file) != sizeof(file_modification_time))
        {
            break;
        }
        if (S_ISDIR(file_mode))
        {
            mkdir(file_name, file_mode);
            if (access(file_name, F_OK) == 0)
            {
                
            }
            else if (mkdir(file_name, file_mode) == -1)
            {
                printf("Failed to create directory '%s'.\n", file_name);
            }
            continue;
        }
        if (fread(&file_size, 1, sizeof(file_size), file) != sizeof(file_size))
        {
            break;
        }
        content = (char *)malloc(file_size * sizeof(char));
        if (fread(content, 1, file_size, file) != file_size)
        {
            break;
        }
        output = fopen(file_name, "wb");
        if (output == NULL)
        {
            perror("Error: ");
            free(content);
            if (fclose(file) == EOF)
            {
                return 0;
            }
            free_inode();
            return 1;
        }
        fwrite(content, 1, file_size, output);
        free(content);
        if (fclose(output) == EOF)
        {
            perror("Error: ");
            if (fclose(file) == EOF)
            {
                return 0;
            }
            free_inode();
            return 1;
        }

        gettimeofday(&mytime, NULL);
        chmod(file_name, file_mode);
        mtime.tv_sec = file_modification_time;
        mtime.tv_usec = 0;
        filetimes[0] = mytime;
        filetimes[1] = mtime;
        if (utimes(file_name, filetimes) == -1)
        {
            perror("Error: ");
            if (fclose(file) == EOF)
            {
                return 0;
            }
            free_inode();
            return 1;
        }
        if (hard_link != NULL)
        {
            hard_link = NULL;
            free(file_name);
        }
    }
    if (fclose(file) == EOF)
    {
        return 0;
    }
    free_inode();
    return 1;
}

void display_tar_contents(const char *tar_filename)
{
    FILE *tar_file;
    uint32_t magic_number;
    uint64_t inode_number;
    uint32_t name_length;
    char *file_name;
    uint32_t file_mode;
    uint64_t file_modification_time;
    uint64_t file_size;
    char *file_contents;
    struct stat file_stat;
    int file_exists;

    tar_file = fopen(tar_filename, "r");
    if (tar_file == NULL)
    {
        perror("Error: Could not open file.\n");
        return;
    }

    fread(&magic_number, 4, 1, tar_file);
    if (magic_number != MAGIC_CONSTANT)
    {
        fprintf(stderr, "Error: Invalid archive format.\n");
        return;
    }

    while (!feof(tar_file))
    {
        if (fread(&inode_number, 8, 1, tar_file) != 1)
        {
            break;
        }
        if (fread(&name_length, 4, 1, tar_file) != 1)
        {
            break;
        }
        file_name = (char *)malloc(name_length + 1);
        if (fread(file_name, name_length, 1, tar_file) != 1)
        {
            break;
        }
        file_name[name_length] = '\0';

        file_exists = stat(file_name, &file_stat);
        if (get_inode(inode_number))
        {
            printf("%s -- inode: %lu\n", file_name, inode_number);
            free(file_name);
            file_name = NULL;
        }
        set_inode(inode_number, file_name);

        if (fread(&file_mode, 1, sizeof(file_mode), tar_file) != sizeof(file_mode))
        {
            break;
        }
        if (fread(&file_modification_time, 1, sizeof(file_modification_time), tar_file) != sizeof(file_modification_time))
        {
            break;
        }

        if (S_ISDIR(file_mode))
        {
            printf("%s/ -- inode: %lu, mode: %o, mtime: %lu\n", file_name, inode_number, file_mode, file_modification_time);
        }
        else if (file_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
        {
            if (fread(&file_size, 8, 1, tar_file) != 1)
            {
                break;
            }
            fseek(tar_file, file_size, SEEK_CUR);
            printf("%s* -- inode: %lu, mode: %o, mtime: %lu, size: %lu\n", file_name, inode_number, file_mode, file_modification_time, file_size);
        }
        else
        {
            if (fread(&file_size, 8, 1, tar_file) != 1)
            {
                break;
            }
            fseek(tar_file, file_size, SEEK_CUR);
            printf("%s -- inode: %lu, mode: %o, mtime: %lu, size: %lu\n", file_name, inode_number, file_mode, file_modification_time, file_size);
        }

        free(file_name);
        file_name = NULL;
    }

    if (fclose(tar_file) == EOF)
    {
        perror("Error while closing file.\n");
        return;
    }
}

int main(int argc, char *argv[])
{

    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s [-cxt] [-f filename] [file]\n", argv[0]);
        return 1;
    }
    char *tar_filename = NULL;
    char mode = NULL;
    int opt;
    int modeCount = 0;

    while ((opt = getopt(argc, argv, "cxtf:")) != -1)
    {
        switch (opt)
        {
        case 'c':
        case 'x':
        case 't':
            if (modeCount > 0)
            {
                fprintf(stderr, "Error: Multiple modes specified\n");
                return 1;
            }
            modeCount++;
            mode = opt;
            break;
        case 'f':
            tar_filename = optarg;
            break;
        default:
            fprintf(stderr, "Usage: %s [-cxt] [-f filename] [file]\n", argv[0]);
            return 1;
        }
    }

    if (tar_filename == NULL)
    {
        fprintf(stderr, "Error: No tarfile specified\n");
        return 1;
    }

    if (mode == NULL)
    {
        fprintf(stderr, "Error: No mode specified\n");
        return 1;
    }

    if (mode == 'c')
    {
        if (argc < optind + 1)
        {
            fprintf(stderr, "Error: No file specified\n");
            return 1;
        }
        create_tar_file(argv[optind], tar_filename);
    }
    else if (mode == 't')
    {
        display_tar_contents(tar_filename);
    }
    else if (mode == 'x')
    {
        extract_tar(tar_filename);
    }
    else
    {
        fprintf(stderr, "Error: Invalid option '%s'\n", mode);
        return 1;
    }

    return 0;
}
