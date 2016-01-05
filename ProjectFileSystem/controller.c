#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "fs.h"

// получение атрибутов файла
int fs_getattr(const char *path, struct stat *stbuf);
// получение содержимого папки
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
// определяем опции открытия файла
int fs_open(const char *path, struct fuse_file_info *fi);
// читаем данные из открытого файла
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
// предоставляет возможность записать в открытый файл
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
// создаём папку
int fs_mkdir(const char *path, mode_t mode);
// создаём файл
int fs_mknod(const char *path, mode_t mode, dev_t dev);
// переименование
int fs_rename(const char *old_path, const char *new_path);
// удалям папку
int fs_rmdir(const char *path);
// удаляем файл
int fs_unlink(const char *path);
// изменить размер файла
int fs_truncate(const char *path, off_t size);


/*
*функция определяет метаинформацию о файле (путь к нему -*path
*метаинформация возвращается в виде структуры stat).
*указатель на функцию передадим модулю фьюз как поле getattr cтруктуры 
*fuse_operations
*/
 
int fs_getattr(const char *path, struct stat *stbuf)
{
	int res = -ENOENT;
	char **nodeNames = parserPath(path);//парсер имя узла
	if (nodeNames != NULL)
	{
		//определяем номер ущла
		int num = searchInode(numRootBlock, nodeNames);
		//записываем инфу об узле в stbuf 
		if (num >= 0 && getInodeStat(num, stbuf) == 0)
		{
			res = 0;
		}
		freeMemoryNodeNames(nodeNames);// освобождение памяти
	}
	return res;
}

/*определяет порядок чтения данных из директории, указатель не нее отдадим в качестве поля readdir*/
/*т.е получаем содержимое каталога, используя указатель на функцию filler*
* вызывается при попытке просмотра содержимого(н-р ls)
*/
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	int res = -ENOENT;
	char **nodeNames = parserPath(path);
	if (nodeNames != NULL)
	{
		//определеяем номер блока с данным узлом
		int num = searchInode(numRootBlock, nodeNames);
		if (num >= 0)
		{
			//получаем блок с папкой
			inode_t *dir = (inode_t *)getBlock(num);
			 if (dir != NULL)
			{
				if (dir->status == BLOCK_IS_dir)
				{
					res = 0;
					filler(buf, ".", NULL, 0);
					filler(buf, "..", NULL, 0);
					char name[NODE_NAME_MAX_SIZE];
					stat_file_t stat;
					int *start = (int *)dir->content;
					int *end = (int *)((void *)dir + sizeBlock);
					//проход по всей папке
					while(start<end)
					{
						//считываем имя и статут узла в переменны
						//если функции выполнились успешно
						 if (*start > 0 && getInodeName(*start, name) == 0 && getInodeStat(*start, &stat) == 0)
						 {
							  if (filler(buf, name, &stat, 0) != 0)
							  {
								  break;
							  }
						 }
						 start++;
					}
				}
				freeMemoryBlock(dir);
			}
		}
		freeMemoryNodeNames(nodeNames);
	}
	return res;
}
 
//определяет имеет ли право пользователь открыть файл /hello, реализуется через анализ данных структуры типа fuse_file_info
int fs_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}
 /*определяет, как именно будет считываться информация из файла для передачи пользователю*/
/*функция чтения данных из открытого файла
*возвращает столько байтов,сколько было запрошено,иначе ошибка
*/
int fs_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi)
{
	int res=-ENOENT;
	//идет ряд операций для получения блока с файлом по заданному адресу
	char **nodeNames = parserPath(path);
	if (nodeNames != NULL)
	{
		int num = searchInode(numRootBlock, nodeNames);
		{
			if (num >= 0)
			{
				inode_t *file = (inode_t *)getBlock(num);
				if (file != NULL)
				{
					if (file->status == BLOCK_IS_FILE)
					{
						if (offset < NODE_CONTENT_MAX_SIZE)
						{
							if (offset + size > NODE_CONTENT_MAX_SIZE)
							{
								size = NODE_CONTENT_MAX_SIZE - offset;
							}
							memcpy(buf, file->content + offset, size);
							res=size;//выполнилось без ошибки
						}
						else {
							res=0;
						}
					}
					freeMemoryBlock(file);
				}
			}
			 freeMemoryNodeNames(nodeNames);
		}
	}
	return res;
}

/*
* запись данных в открытый файл, возвращает кол-во записанных байтов, должно ра
передан. кол-ву, иначе ошибка.
*/
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int res = -ENOENT;
	//ряд операций для получения блока с заданным адресом
	char **nodeNames = parserPath(path);
	if (nodeNames != NULL)
	{
		int num = searchInode(numRootBlock, nodeNames);
		if (num >= 0)
		{
			inode_t *file = (inode_t *)getBlock(num);
			if (file != NULL)
			{
				if (file->status == BLOCK_IS_FILE)
				{
					if (offset < NODE_CONTENT_MAX_SIZE)
					{
						if (offset + size > NODE_CONTENT_MAX_SIZE)
						{
							 size = NODE_CONTENT_MAX_SIZE - offset;
						}
						memcpy(file->content + offset, buf, size);
						if (file->stat.st_size < offset + size)
						{
							 file->stat.st_size = offset + size;
						}
						if (writeBlock(num, file) == 0)
						{
							res=size;
						}
					}
					else 
					{
						res=0;
					}
				}
				freeMemoryBlock(file);
			}
		}
		freeMemoryNodeNames(nodeNames);
	}
	return res;
}

/*создаем директорий*/
int fs_mkdir(const char *path, mode_t mode)
{
	int res=-ENOENT;
	//получаем блок с заданным адресом
	char **nodeNames = parserPath(path);
	if (nodeNames != NULL)
	{
		//без имени последнего узла
		char *name = excludeLastNodeName(nodeNames);
		if (name != NULL)
		{
			//определяем номер узла.
			int dir_num = searchInode(numRootBlock, nodeNames);
			if (dir_num >= 0)
			{
				//создаем папку
				int new_dir = createDir(name, mode);
				if (new_dir >= 0 && addInodeToDir(dir_num, new_dir) == 0)
				{
					//добавили узел в папку по номеру папки и номеру узла
					res = 0;
				}
			}
			 freeMemoryName(name);
		}
		freeMemoryNodeNames(nodeNames);
	}
	return res;
}

/*
*Создает узел file.будет вызываться для создания узлов, 
* отличных от каталога и символических ссылок.
*/
int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
	int res = -ENOENT;
	char **nodeNames = parserPath(path);
	if (nodeNames != NULL)
	{
		char *name = excludeLastNodeName(nodeNames);
		if (name != NULL)
		{
			int dir_num = searchInode(numRootBlock, nodeNames);
			if (dir_num >= 0)
			{
				//создаем узел file
				int new_file = createFile(name, mode, dev);
				if (new_file >= 0 && addInodeToDir(dir_num, new_file) == 0)
				{
					res=0;
				}
			}
			freeMemoryName(name);
		}
		freeMemoryNodeNames(nodeNames);
	}
	return res;
}

/*
*переименовывание файла
*т.е по сути переименовывание-это изменение адреса
*/
int fs_rename(const char *old_path, const char *new_path)
{
	int res = -ENOENT;
	char **old_nodeNames = parserPath(old_path);
	if (old_nodeNames != NULL)
	{
		 char **new_nodeNames = parserPath(new_path);
		if (new_nodeNames != NULL)
		{
			char *old_name = excludeLastNodeName(old_nodeNames);
			if (old_name != NULL)
			{
				char *new_name = excludeLastNodeName(new_nodeNames);
				if (new_name != NULL)
				{
					int old_dir_num = searchInode(numRootBlock, old_nodeNames);
					int new_dir_num = searchInode(numRootBlock, new_nodeNames);
					int node_num = searchInodeInDir(old_dir_num, old_name);
					removeNodeFromDir(old_dir_num, node_num);
					addInodeToDir(new_dir_num, node_num);
					setInodeName(node_num, new_name);
					res= 0;
					freeMemoryName(new_name);
				}
				freeMemoryName(old_name);
			}
			freeMemoryNodeNames(new_nodeNames);
		}
		freeMemoryNodeNames(old_nodeNames);
	}
	return res;
}

//удаление директории
int fs_rmdir(const char *path)
{
	int res = -ENOENT;
	char **nodeNames = parserPath(path);
	if (nodeNames != NULL)
	{
		char *name = excludeLastNodeName(nodeNames);
		if (name != NULL)
		{
			int dir_num = searchInode(numRootBlock, nodeNames);
			if (dir_num >= 0)
			{
				int node_num = searchInodeInDir(dir_num, name);
				if (node_num >= 0)
				{
					if (removeNodeFromDir(dir_num, node_num) == 0 && removeBlock(node_num) == 0)
					{
						res=0;
					}
					
				}
			}
			freeMemoryName(name);
		}
		freeMemoryNodeNames(nodeNames);
	}
	return res;
}

// удаление файла
int fs_unlink(const char *path)
{
	int res = -ENOENT;
	char **nodeNames = parserPath(path);
	if (nodeNames != NULL)
	{
		char *name = excludeLastNodeName(nodeNames);
		if (name != NULL)
		{
			int dir_num = searchInode(numRootBlock, nodeNames);
			if (dir_num >= 0)
			{
				int node_num = searchInodeInDir(dir_num, name);
				if (node_num >= 0)
				{
					if (removeNodeFromDir(dir_num, node_num) == 0 && removeBlock(node_num) == 0)
					{
						res=0;
					}
				}
			}
			freeMemoryName(name);
		}
		freeMemoryNodeNames(nodeNames);
	}
	return res;
}

//изменение размера файла
int fs_truncate(const char *path, off_t size)
{
	int res = -ENOENT;
	char **nodeNames = parserPath(path);
	if (nodeNames != NULL)
	{
		int num = searchInode(numRootBlock, nodeNames);
		if (num >= 0)
		{
			stat_file_t stat;
			if (getInodeStat(num, &stat) == 0)
			{
				if (size <= NODE_CONTENT_MAX_SIZE)
				{
					stat.st_size = size;
					if (setInodeStat(num, &stat) == 0)
					{
						res=0;
					}
				}
			}
		}
		freeMemoryNodeNames(nodeNames);
	}
	return res;
}


struct fuse_operations fs_oper= 
{
	.getattr    = fs_getattr,
	.readdir    = fs_readdir,
	.open       = fs_open,
	.read       = fs_read,
	.write      = fs_write,
	.mkdir      = fs_mkdir,
	.mknod      = fs_mknod,
	.rename     = fs_rename,
	.rmdir      = fs_rmdir,
	.unlink     = fs_unlink,
	.truncate   = fs_truncate,
};	//необходимая для создания файловой системы переменная структуры с типом fuse_operations, будет необходимо передать ее в функцию fuse_main

/*структура fuse_operations передает указатель на функции, 
* которые будут вызываться для выполнения 
соответ. действия */

/*то есть создаем небходимые функции с логикой их выполнения,
* затем создаем переменную fuse_operations и отдаем ей 
* соотв. функций,которые необходимо будет использовать*/
int main(int argc, char *argv[])
{
    if (load() != 0)
    {
        printf("load error\n");
        return -1;
    }
    return fuse_main(argc, argv, &fs_oper, NULL);
}