#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "fs.h"

const int sizeBlock = SIZE_BLOCK;
int fs_fd = -1;
const int numRootBlock = NUMROOTBLOCK;


//загружаем данные из файла и инициализация глоб переменных
/*если файл не создан,то создаем файл с фс*/
int load()
{
	int res=0;
	/*попытка открыть существующий файл с фс, одновременно для чтения
	* и записи
	*/
	fs_fd=open(FILESYSTEM, O_RDWR, 0666);
	if(fs_fd<0)
	{
		//если не создан, создаем новый файл с фс
		fs_fd=open(FILESYSTEM, O_CREAT | O_RDWR, 0666);
		//создаем корень
		if (fs_fd<0||createRoot()!=0)
			res=-1;
	}
	return res;//0-успешное завершение
}
 
//создаем корень
int createRoot()
{
	int res=-1;
	inode_t *root=(inode_t *)createBlock();
	if (root!=NULL)
	{
		//корневая директория
		root->status = BLOCK_IS_dir;
		root->name[0] = '\0';
		root->stat.st_mode = S_IFDIR | 0777;
		root->stat.st_nlink = 2;
		if (writeBlock(numRootBlock, root) == 0)
		{
			res = 0;
		}
		freeMemoryBlock(root);
	}
	return res;
}

void *createBlock()
{
	//возвращает указатель на первый байт выделенной области
	return calloc(sizeBlock, sizeof(char));
}

void freeMemoryBlock(void *block)
{
    free(block);//освобождаем память
}

int readBlock(int num, void *block)
{
	int res=-1;
	//смещение от начала файла в байтах
	if (num>=0&&lseek(fs_fd, sizeBlock * num, SEEK_SET) >= 0)
	{
		//cчитываем
		if (read(fs_fd,block,sizeBlock)>=0)
		{
			res=0;
		}
	}
	return res;
}

int writeBlock(int num,void *block)
{
	int res = -1;
	if (num >= 0 && lseek(fs_fd, sizeBlock * num, SEEK_SET) >= 0)
	{
		if (write(fs_fd, block, sizeBlock) == sizeBlock)
		{
			res = 0;
		}
	}
	return res;
}

//ищем первый свободный блок
int searchFreeBlock()
{
	int num=numRootBlock+1; //номер следования
	char status;
	int read_res;
	while(TRUE)
	{
		//выход за границы файла
		if (lseek(fs_fd,  sizeBlock * num, SEEK_SET) < 0)
		{
			num = -1;
			break;
		}
		read_res=read(fs_fd,&status,sizeof(char));
		if (read_res < 0)
		{
			num = -1;
			break;
		}
		//конец файла или нашли свободный блок
		if (read_res == 0 || status == BLOCK_IS_FREE)
		{
			break;
		}
		//нашли номер этого блока
		num++;
	}
	return num;
}

//получаем блок по его номеру
void *getBlock(int num)
{
	void *block = NULL;
	if (num >= 0)
	{
		//инициализируем блок
		block=createBlock();
		if(block!=NULL&&readBlock(num,block)!=0)
		{
			freeMemoryBlock(block);
			block=NULL;
		}
	}
	return block;
}

//заносим инфу о статусе
int setBlockStatus(int num, char status)
{
	int res=-1;
	if (num >= 0 && lseek(fs_fd, sizeBlock * num + BLOCK_STATUS_OFFSET, SEEK_SET) >= 0)
	{
		//если запись без ошибок
		if (write(fs_fd, &status, sizeof(char)) == sizeof(char))
			res=0;
	}
	return res;
}

//удалить блок 
int removeBlock(int num)
{
	int res=0;
	int status=getBlockStatus(num);
	//действия в зависимости от того,чем является блок
	switch(status) 
	{
		case BLOCK_IS_FREE:
			break;
		case BLOCK_IS_dir:
			removeDir(num);//удалить папку 
			break;
		case BLOCK_IS_FILE:
			removeFile(num);
			break;
		default:
			res= -1;
			break;
	}
	return res;
}

//поиск узла
//если найден,возвращает его номер
int searchInode(int node_num,char **nodeNames)
{
	int res=-1;
	//узел существует
	if (node_num >= 0 && nodeNames != NULL)
	{
		if (*nodeNames == NULL)//имя узла не задано
		{
			res= node_num;
		}
		else 
		{
			//ищем след свободный узел
			int next_node_num = searchInodeInDir(node_num, *nodeNames);
			if (next_node_num > 0)
			{
				res = searchInode(next_node_num, nodeNames + 1);
			}
		}
	}
	return res;
}

//удаление файла
//то есть помечаем блок,где хранился файл как свободный
int removeFile(int num)
{
    return setBlockStatus(num, BLOCK_IS_FREE);
}

//удаление директории
int removeDir(int num)
{
	int res=-1;
	inode_t *dir = (inode_t *)getBlock(num);
	if (dir != NULL)//если папка не пуста
	{
		/*определяем начало и конец*/
		int *start = (int *)dir->content;
		int *end = (int *)((void *)dir + sizeBlock);
		//проход по папке 
		while (start < end)
		{
			if (*start > 0)
			{
				removeBlock(*start);
			}
			start++;
		}
		freeMemoryBlock(dir);//освобожиди память
		res = setBlockStatus(num, BLOCK_IS_FREE);
	}
	return res;
}

int createDir(const char *name, mode_t mode)
{
	//ищем номер свободного блока
	int num = searchFreeBlock();
	if (num >= 0)
	{
		//выделяем память
		inode_t *dir = (inode_t *)createBlock();
		if (dir != NULL)
		{
			int name_size = strlen(name) + 1;
			if (name_size > NODE_NAME_MAX_SIZE)
			{
				name_size = NODE_NAME_MAX_SIZE;
			}
			//флаг,что блок является папкой
			dir->status = BLOCK_IS_dir;
			memcpy(dir->name, name, name_size);
			dir->stat.st_mode = S_IFDIR | mode;
			dir->stat.st_nlink = 2;
			if (writeBlock(num, dir) != 0)
			{
				num = -1;
			}
			freeMemoryBlock(dir);//освобождаем память
			
		}
	}
	return num;
}

int createFile(const char *name, mode_t mode, dev_t dev)
{
	int num = searchFreeBlock();
	if (num >= 0)
	{
		inode_t *file = (inode_t *)createBlock();
		if (file != NULL)
		{
			int name_size = strlen(name) + 1;
			if (name_size > NODE_NAME_MAX_SIZE)
			{
				name_size = NODE_NAME_MAX_SIZE;
			}
			file->status = BLOCK_IS_FILE;
			memcpy(file->name, name, name_size);
			file->stat.st_mode = S_IFREG | mode;
			file->stat.st_rdev = dev;
			file->stat.st_nlink = 1;
			if (writeBlock(num, file) != 0)
			{
				num = -1;
			}
		freeMemoryBlock(file);
        }
    }
    return num;
}

//парсер адреса
char **parserPath(const char *path)
{
	char **res = NULL;
	int path_size = strlen(path) + 1;
	char *copy_path = (char *)malloc(path_size);
	if (copy_path != NULL)
	{
		memcpy(copy_path, path, path_size);
		int depth = 0;//вложенность
		int i = 0;
		//пока не дошли до корневого
		while (copy_path[i] != '\0')
		{
			if (copy_path[i] == '/')
			{
				depth++;
				copy_path[i] = '\0';
			}
			i++;
			
		}
		if (copy_path[i - 1] == '\0')
		{
			depth--;
		}
		res = (char **)malloc(sizeof(char **) * (depth + 1));
		if (res != NULL)
		{
			i = 0;
			int j = 0;
			while (j < depth)
			{
				while (copy_path[i++] != '\0');
				res[j++] = createName(copy_path + i);
			}
			res[j] = NULL;
		}
		free(copy_path);
	}
	return res;
}

int getBlockStatus(int num)
{
	int res=-1;
	if (num >= 0 && lseek(fs_fd, sizeBlock * num + BLOCK_STATUS_OFFSET, SEEK_SET) >= 0)
	{
		char status;
		res= read(fs_fd, &status, sizeof(char));
		 if (res < 0)
		{
			res = -1;
		}
		else if (res == 0)//считано 0байт
		{
			res = BLOCK_IS_FREE;
		}
		else
		{
			res = status;//присваиваем полученное при считывании значение
			
		}
	}
	return res;
}

int getInodeName(int num, char *buf)
{
	int res = -1;
	if (num >= 0 && lseek(fs_fd, sizeBlock * num + NODE_NAME_OFFSET, SEEK_SET) >= 0)
	{
		//считываем в переменную и проверяем на ошибки
		if (read(fs_fd, buf, NODE_NAME_MAX_SIZE) == NODE_NAME_MAX_SIZE)
		{
			res = 0;
		}
	}
	return res;
}

int getInodeStat(int num, stat_file_t *stbuf)
{
	int res = -1;
	if (num >= 0 && lseek(fs_fd, sizeBlock * num + NODE_STAT_OFFSET, SEEK_SET) >= 0)
	{
		if (read(fs_fd, stbuf, sizeof(stat_file_t)) == sizeof(stat_file_t))
		{
			res = 0;
		}
	}
	return res;
}

//*buf-указатель на заданное имя
int setInodeName(int num, char *buf)
{
	int res= -1;
	if (num >= 0 && lseek(fs_fd, sizeBlock * num + NODE_NAME_OFFSET, SEEK_SET) >= 0)
	{
		if (write(fs_fd, buf, NODE_NAME_MAX_SIZE) == NODE_NAME_MAX_SIZE)
		{
			res = 0;
		}
	}
	return res;
}

int setInodeStat(int num, stat_file_t *buf)
{
	int res = -1;
	if (num >= 0 && lseek(fs_fd, sizeBlock * num + NODE_STAT_OFFSET, SEEK_SET) >= 0)
	{
		if (write(fs_fd, buf, sizeof(stat_file_t)) == sizeof(stat_file_t))
		{
			res = 0;
		}
	}
	return res;
}

char *createName(const char *name)
{
	char *res = (char *)calloc(NODE_NAME_MAX_SIZE, sizeof(char));
	if (res != NULL)
	{
		int size = strlen(name) + 1;
		if (size > NODE_NAME_MAX_SIZE)
		{
			size = NODE_NAME_MAX_SIZE;
		}
		memcpy(res, name, size);
	}
	return res;
	
}

char *createEmptyName()
{
	return (char *)calloc(NODE_NAME_MAX_SIZE, sizeof(char));
}

void freeMemoryName(char *name)
{
    free(name);
}

// исключить имя последнего узла
char *excludeLastNodeName(char **nodeNames)
{
	char *result = NULL;
	if (nodeNames != NULL && *nodeNames != NULL)
	{
		 while (nodeNames[1] != NULL)
		{
			nodeNames++;
		}
		result = nodeNames[0];
		nodeNames[0] = NULL;
	}
	return result;
}

void freeMemoryNodeNames(char **nodeNames)
{
	if (nodeNames != NULL)
	{
		char **tmp = nodeNames;
		while (*tmp != NULL)
		{
			freeMemoryName(*tmp);
			tmp++;
		}
		free(nodeNames);
	}	
}

int clearBlock(int num)
{
	int res = -1;
	if (num >= 0 && lseek(fs_fd, sizeBlock * num, SEEK_SET) >= 0)
	{
		void *block = createBlock();
		if (block != NULL)
		{
			if (writeBlock(num, block) == 0)
			{
				res = 0;
			}
			freeMemoryBlock(block);
		}
	}
	return res;
}

int addInodeToDir(int dir_num, int node_num)
{
	int res = -1;
	if (dir_num >= 0 && node_num > 0)
	{
		inode_t *dir = (inode_t *)getBlock(dir_num);
		if (dir != NULL)
		{
			if (dir->status == BLOCK_IS_dir)
			{
				int *start = (int *)dir->content;
				int *end = (int *)((void *)dir + sizeBlock);
				//проход по папке
				while (start < end)
				{
					if (*start <= 0)
					{
						*start = node_num;
						break;
					}
					start++;
				}
				if (start < end)
				{
					res= writeBlock(dir_num, dir);
				}
			}
			freeMemoryBlock(dir);
			
		}
	}
	return res;
}

int removeNodeFromDir(int dir_num, int node_num)
{
	int res= -1;
	if (dir_num >= 0 && node_num > 0)
	{
		inode_t *dir = (inode_t *)getBlock(dir_num);
		if (dir != NULL)
		{
			if (dir->status == BLOCK_IS_dir)
			{
				int *start = (int *)dir->content;
				int *end = (int *)((void *)dir + sizeBlock);
				while (start < end)
				{
					if (*start == node_num)
					{
						*start = 0;
						break;
					}
					start++;
				}
			if (start < end)
			{
				res = writeBlock(dir_num, dir);
			}
			else
			{
				res = 0;
			}
            }
            freeMemoryBlock(dir);
        }
    }
    return res;
}

int searchInodeInDir(int dir_num, const char *node_name)
{
	int result = -1;
	if (dir_num >= 0 && node_name != NULL)
	{
		inode_t *dir = (inode_t *)getBlock(dir_num);
		if (dir != NULL)
		{
			if (dir->status == BLOCK_IS_dir)
			{
				char name[NODE_NAME_MAX_SIZE];
				int *start = (int *)dir->content;
				int *end = (int *)((void *)dir + sizeBlock);
				while (start < end)
				{
					if (*start > 0 && getInodeName(*start, name) == 0 && strcmp(node_name, name) == 0)
					{
						result = *start;
						break;
					}
					start++;
				}
			}
			freeMemoryBlock(dir);
		}
	}
	return result;
}