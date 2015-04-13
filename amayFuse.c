/*
PLEASE FOLLOW THESE INSTRUCTIONS ONLY OTHERWISE IT WON'T WORK

Whereever you work from please let the code have priviliges to create a directory in that folder.

I have not created or written into or done anything via shell;
All of my file system works as a physical drive
You can write through opening the file on explorer. I think you can create directories and files using the shell.
You can acccess it through the EXPLORER.

I did not want to mount my file system on mnt cause of priviliges and what not reasons I have just mounted it on the whole computer drive and then used 
/tmp as my base my operation. So no privilege hassles.

NOTE:  I have used the tmp folder as my root folder so my file system resides in "/tmp" folder in the root.
Also Please don't make directories and files of same name in a directory.

Compile :  gcc -Wall -w amayFuse.c `pkg-config fuse --cflags --libs` -lulockmgr -o amayFuse
RUN:   ./amayFuse
AFTER END ALWAYS UNMOUNT: fusermount -u Amay

The system mounts the root itself i.e. the "/" but "/tmp" is where the file system is.

Wherever you run it from make sure the priviliges are for my code to make it's directory where all the fusedata.XXXX files are stored

NOTE: I get an error if I rename  and then wite on the file but if you stop the processes and then remount the filesystem then it seems to work.

FUSE uses some sort of /.goutputstream-XXXXX to write to a file and that messes up the the entire system to I had to create a seperate buffer and that's complete prone to errors.


*/

#define FUSE_USE_VERSION 26
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

#include <fuse.h>
#include <ulockmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <sys/file.h> /* flock(2) */

#define BLOCK_SIZE 4096
#define MAX_BLOCKS 10000
#define MAX_FILE_SIZE 1638400

static int freeblocks[MAX_BLOCKS] = { 0 };
static int size = 0;
static int write_buf_flag = 0;
static char *buffer;

static void getFreeBlocks(){
	FILE *fd = NULL;
	char *addedPath = "/fusedata.";
	char str[5];
	char num[5];
	int i =0 , j = 0 , pos = 0 , number = 0;
	char *fileContent = (char *) malloc(BLOCK_SIZE);
	char *tmpPath=(char *) malloc(30);
	for(i = 1; i < 26 ;i++){
		strcpy(tmpPath,"FileSysData");
		sprintf(str, "%d", i);
		strcat(tmpPath,addedPath);
		char *filename = strcat(tmpPath,str);
		fd = fopen(filename,"r");
		fscanf(fd, "%s", fileContent);		
		j = 1;
		while(fileContent[j] != '}'){
			pos = 0;
			while(fileContent[j] != ','){	
				num[pos++] = fileContent[j++];						
			}
			sscanf(num, "%d", &number);
			freeblocks[number] = 1;	
			size ++;
			sprintf(num, "%d", 0);
			j++;			
		}		
		fclose(fd);
	}		
	free(fileContent);
	free(tmpPath);	
}

static void setFreeBlocks(int block){
	freeblocks[block] = 0;
	int freeBlock = (block/400)+1;
	char str[5];	
	char *tmpPath=(char *) malloc(30);
	sprintf(str, "%d", freeBlock);
	strcpy(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,str);
	FILE *fd = NULL;
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%s", fileContent);
	fclose(fd);
	sprintf(str, "%d", block);
	char *point = strstr (fileContent,str);
	int pos1 = point - fileContent;
	int pos2 = pos1;
	while(fileContent[pos2++]!=','){}
	char* substr  = (char *) malloc(BLOCK_SIZE);
	strcpy(substr,"");
	strncat(substr, fileContent, pos1);
	strcat(substr, fileContent+pos2);
	fd = fopen(tmpPath,"w");
	fprintf(fd, "%s", substr);	
	fclose(fd);
	free(substr);
	free(fileContent);
	free(tmpPath);
}

static void addFreeBlocks(int block){
	if(block == 0){return;}
	freeblocks[block] = 0;
	int freeBlock = (block/400)+1;
	char str[5];	
	char *tmpPath=(char *) malloc(30);
	sprintf(str, "%d", freeBlock);
	strcpy(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,str);
	FILE *fd = NULL;
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%s", fileContent);
	fclose(fd);
	sprintf(str, "%d", block);
	char *point = strstr (fileContent,"}");
	int pos1 = point - fileContent;
	char* substr  = (char *) malloc(BLOCK_SIZE);
	strcpy(substr,"");
	strncpy(substr, fileContent, pos1);
	strcat(substr,str);
	strcat(substr,",");
	strcat(substr,"}");	
	fd = fopen(tmpPath,"w");
	fprintf(fd, "%s", substr);
	fclose(fd);
	free(substr);
	free(fileContent);
	free(tmpPath);
}


static int getOneBlock(){	
	int i = 0;
	for(i = 0; i < MAX_BLOCKS ;i++){
		if(freeblocks[i] == 1){
			char str[5];	
			char *tmpPath=(char *) malloc(30);
			sprintf(str, "%d", i);
			strcpy(tmpPath,"FileSysData/fusedata.");
			strcat(tmpPath,str);
			FILE *fd = NULL;	
			fd = fopen(tmpPath,"w");
			fprintf(fd, "%s", "");
			fclose(fd);
			free(tmpPath);
			return i;
		}	
	}
	return -1;
}

static int getThatBlock(const char *path){
	int count = 0 , i = 0 , pos = 0;
	while(path[i]!='\0'){
		if(path[i]=='/'){
			count++;
		}i++;
	}	
	char folders[count][20];
	i = 1;
	char ch[20];
	int tmp = 0;
	while(path[i]!='\0'){
		if(path[i]=='/'){
			i++;				
			strcpy(folders[pos++],ch);
			for(tmp = 0 ; tmp < 20 ; tmp++){
				ch[tmp] = '\0';
			}
			tmp = 0;	
		}
		ch[tmp++]=path[i++];
	}
	FILE *fd = NULL;
	char *fileContent = (char *) malloc(BLOCK_SIZE);
	char *addedPath = "fusedata.";
	char str[5];
	char *tmpPath=(char *) malloc(30);
	int fileNum = 26;
	for(i = 1 ; i < pos ; i++){		
		strcpy(tmpPath,"FileSysData/");		
		sprintf(str, "%d", fileNum);
		strcat(tmpPath,addedPath);		
		char *filename = strcat(tmpPath,str);
		fd = fopen(filename,"r");
		fscanf(fd, "%[^\n]s", fileContent);	
		fclose(fd);					
		strcpy(filename,"");		
		char *folderPoint = strstr(fileContent,folders[i]);
		int pos1 = folderPoint - fileContent + strlen(folders[i]) +1;
		int pos2 = pos1;
		while(1){
			if(fileContent[pos2]=='}' || fileContent[pos2]==','){
				break;
			}pos2++;
		}
		char* substr  = (char *) malloc(10);
		strncpy(substr, fileContent+pos1, pos2-pos1);	
		fileNum = atoi(substr);	
	}
	free(fileContent);
	free(tmpPath);	
	return fileNum;		
}

static void makedir(const char *path){	
	/*char *p = strstr(path,"/.");	
	int p1 =p-path;
	if(p1>=0){printf("returned\n");return;}	*/
	int block = getOneBlock();
	setFreeBlocks(block);
	int rootblocknum = getThatBlock(path);
	if(block == -1){
		printf("file system filled up");
		return;
	}
	char str[5];
	sprintf(str, "%d", block);
	FILE *fd = NULL;
	time_t seconds = time(NULL);
	char *tmpPath=(char *) malloc(30);
	strcpy(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,str);	
	fd = fopen(tmpPath,"w");	
	fprintf(fd,"{size:0,uid:1000,gid:1000,mode:16877,atime:%ld,ctime:%ld,mtime:%ld,linkcount:2,filename_to_inode_dict:{d:.:%d,d:..:%d}}",seconds,seconds,seconds,block,rootblocknum);
	fclose(fd);	
	sprintf(str, "%d", rootblocknum);		
	strcpy(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,str);
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%[^\n]s", fileContent);
	fclose(fd);
	char *point = strstr (fileContent,"size:");
	int pos1 = point - fileContent + 5;
	int pos2 = pos1;
	while(fileContent[pos2++]!=','){}
	char* substr  = (char *) malloc(BLOCK_SIZE);	
	strncpy(substr, fileContent+pos1, pos2-pos1-1);	
	int sizeblock = atoi(substr);	
	point = strstr (fileContent,"linkcount:");
	pos1 = point - fileContent + 10;
	pos2 = pos1;
	while(fileContent[pos2++]!=','){}
	strncpy(substr, fileContent+pos1, pos2-pos1-1);	
	int links = atoi(substr);
	point = strstr (fileContent,"filename_to_inode_dict:{");
	pos1 = point - fileContent + 24;
	pos2 = pos1;
	while(fileContent[pos2++]!='}'){}
	strcpy(substr,"");
	strncat(substr, fileContent+pos1, pos2-pos1-1);
	char* lastPath  = (char *) malloc(20);
	int tmp =0;
	char ch[20];
	int i = 0;
	while(path[i]!='\0'){
		if(path[i]=='/'){
			i++;
			for(tmp = 0 ; tmp < 20 ; tmp++){
				ch[tmp] = '\0';
			}		
			tmp = 0;			
		}
		ch[tmp++]=path[i++];
	}
	strcpy(lastPath,ch);
	strcat(substr,",d:");
	strcat(substr,lastPath);
	strcat(substr,":");
	sprintf(str, "%d", block);
	strcat(substr,str);	
	fd = fopen(tmpPath,"w");	
	fprintf(fd, "{size:%d,uid:1000,gid:1000,mode:16877,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,filename_to_inode_dict:{%s}}",(sizeblock+1),seconds,seconds,seconds,(links+1),substr);
	fclose(fd);
	free(fileContent);
	free(substr);	
	free(lastPath);
	free(tmpPath);
}

static void removeFile(int block){	
	FILE *fd = NULL;
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	char str[5];
	sprintf(str, "%d", block);
	char *tmpPath=(char *) malloc(30);
	strcpy(tmpPath, "FileSysData/fusedata.");
	strcat(tmpPath, str);
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%[^\n]s", fileContent);
	fclose(fd);	
	char *point = strstr (fileContent,"indirect:");
	int pos1 = point - fileContent + 9;
	char ch = fileContent[pos1];
	char *substr = (char *) malloc(10);	
	point = strstr (fileContent,"location:");
    pos1 = point - fileContent + 9;
	int pos2 = pos1;	
	while(fileContent[pos2++]!='}'){}
	strcpy(substr, "");
	strncat(substr, fileContent+pos1,pos2-pos1-1);		
	if(ch  == '0'){
		addFreeBlocks(block);
		int tmp = atoi(substr);
		addFreeBlocks(tmp);
	}else{
		addFreeBlocks(block);
		strcpy(tmpPath, "FileSysData/fusedata.");
		strcat(tmpPath, substr);
		strcpy(fileContent,"");	
		fd = fopen(tmpPath,"r");
		fscanf(fd,"%[^\n]s", fileContent);
		fclose(fd);
		int numFiles = 0;
		pos2 =0;
		while(fileContent[pos2]!='}'){if(fileContent[pos2]==','){numFiles++;}pos2++;}		
		int listofFile[numFiles];	
		int tmp = 0;
		pos1 = 0;
		for(tmp = 0 ; tmp < 5 ; tmp++){
			str[tmp] = '\0';
		}tmp = 0;
		int i = 1 ;
		while(fileContent[i]!='}'){						
			if(fileContent[i]==','){
				i++;
				;	
				listofFile[pos1++] = atoi(str);

				for(tmp = 0 ; tmp < 5 ; tmp++){
					str[tmp] = '\0';
				}tmp=0;
				if(fileContent[i]=='}'){break;}
			}
			str [tmp++] = fileContent[i++];
		}
		for(i = 0 ; i < pos1 ; i++){	
			addFreeBlocks(listofFile[i]);	
		}
	}	
	free(fileContent);
	free(tmpPath);
	free(substr);

}

static void removeDirectories(int block){	
	addFreeBlocks(block);
	FILE *fd = NULL;
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	char str[5];
	sprintf(str, "%d", block);
	char *tmpPath=(char *) malloc(30);
	strcpy(tmpPath, "FileSysData/fusedata.");
	strcat(tmpPath, str);
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%[^\n]s", fileContent);
	fclose(fd);		
	int tmpComma = 0;
	char *point = strstr(fileContent,"filename_to_inode_dict:{");
	int pos1 = point - fileContent;
	while(fileContent[pos1]!='}' && tmpComma < 2){
		if(fileContent[pos1]==','){ 
			tmpComma++;
		} pos1++;
	}	
	int pos2 = point - fileContent;
	if(fileContent[pos1]=='}'){return;}
	pos1--;			
	while(fileContent[pos1]!='}'){
		if(fileContent[pos1] == ','){
			if(fileContent[pos1+1] == 'd'){
				pos2 = pos1+3;				
				while(fileContent[pos2++]!=':'){}				
				pos1 = pos2;
				while(1){					
					if(fileContent[pos2]==',' || fileContent[pos2]=='}'){break;}pos2++;
				}
				strncpy(str,fileContent+pos1 , pos2-pos1);
				block = atoi(str);				
				removeDirectories(block);				
				pos1 = pos2;								
			}else if(fileContent[pos1+1] == 'f'){
				pos2 = pos1+3;
				while(fileContent[pos2++]!=':'){}
				pos1 = pos2;				
				while(1){					
					if(fileContent[pos2]==',' || fileContent[pos2]=='}'){break;}pos2++;
				}
				strncpy(str,fileContent+pos1 , pos2-pos1);
				block = atoi(str);			
				removeFile(block);				
				pos1 = pos2;
			}
		}
	}
	free(fileContent);	
}

static void removeDir(const char *path, int fileDir){	
	int rootblocknum = getThatBlock(path);
	if(rootblocknum == -1){
		printf("not present");
		return -1;
	}	
	time_t seconds = time(NULL);
	char str[5];
	char *tmpPath=(char *) malloc(30);
	sprintf(str, "%d", rootblocknum);		
	strcat(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,str);
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	FILE *fd = NULL;
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%[^\n]s", fileContent);
	fclose(fd);
	char *point = strstr (fileContent,"size:");
	int pos1 = point - fileContent + 5;
	int pos2 = pos1;
	while(fileContent[pos2++]!=','){}
	char* substr  = (char *) malloc(BLOCK_SIZE);	
	strncpy(substr, fileContent+pos1, pos2-pos1-1);	
	int sizeblock = atoi(substr);	
	point = strstr (fileContent,"linkcount:");
	pos1 = point - fileContent + 10;
	pos2 = pos1;
	while(fileContent[pos2++]!=','){}
	strncpy(substr, fileContent+pos1, pos2-pos1-1);	
	int links = atoi(substr);
	char* lastPath  = (char *) malloc(20);
	int tmp =2;
	char ch[20];
	if(fileDir == 0){
		ch[0] = 'd';
	}else{
		ch[0] = 'f';
	}
	ch[1] = ':';
	int i = 0;
	while(path[i]!='\0'){
		if(path[i]=='/'){
			i++;
			for(tmp = 2 ; tmp < 20 ; tmp++){
				ch[tmp] = '\0';
			}
			tmp = 2;			
		}
		ch[tmp++]=path[i++];
	}	
	strcpy(lastPath,ch);	
	point = strstr (fileContent,lastPath);	
	int posBlock = point - fileContent + strlen(lastPath)+1;
	for(tmp = 0 ; tmp < 20 ; tmp++){
		ch[tmp] = '\0';
	}tmp=0;
	while(1){
		if(fileContent[posBlock]=='}'|| fileContent[posBlock]==','){break;}
		ch[tmp++] = fileContent[posBlock++];
	}		
	int block = atoi(ch);
	printf("block %d\n", block);
	if(fileDir == 0){
		removeDirectories(block);
	}else{
		removeFile(block);
	}	
	char *point2 = strstr (fileContent,"filename_to_inode_dict:{");
	pos1 = point - fileContent;
	pos2 = point2 - fileContent + 24;
	int pos3 = pos1;
	while(1){
		if(fileContent[pos3]=='}' || fileContent[pos3]==','){
			break;
		}pos3++;
	}
	strncpy(substr, fileContent+pos2, pos1-pos2-1);
	strcat(substr, fileContent+pos3);			
	fd = fopen(tmpPath,"w");
	fprintf(fd, "{size:%d,uid:1000,gid:1000,mode:16877,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,filename_to_inode_dict:{%s",(sizeblock-1),seconds,seconds,seconds,(links-1),substr);			
	fclose(fd);
	free(fileContent);
	free(substr);	
	free(lastPath);
	free(tmpPath);
}	

static void renameDir(const char *from, const char *to){	
	char *p = strstr(from,"/.goutputstream");	
	int p1 =p-from;
	if(p1>=0){printf("renmae returned\n");return;}
	int rootblocknum = getThatBlock(from);
	char* lastPath  = (char *) malloc(20);
	int tmp =0;
	char ch[20];	
	int i = 0;
	while(from[i]!='\0'){
		if(from[i]=='/'){
			i++;
			for(tmp = 0 ; tmp < 20 ; tmp++){
				ch[tmp] = '\0';
			}
			tmp = 0;			
		}
		ch[tmp++]=from[i++];
	}		
	strcpy(lastPath,ch);	
	char str[5];
	char *tmpPath=(char *) malloc(30);
	sprintf(str, "%d", rootblocknum);		
	strcat(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,str);
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	FILE *fd = NULL;
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%[^\n]s", fileContent);
	fclose(fd);
	char *point = strstr (fileContent,"filename_to_inode_dict:{");
	int pos1 = point - fileContent ;
	char* substr  = (char *) malloc(BLOCK_SIZE);
	strncat(substr, fileContent, pos1+24);
	point = strstr (fileContent,lastPath);
	int pos2 = point - fileContent;	
	strncat(substr,  fileContent+pos1+24,pos2-pos1-24);
	while(fileContent[pos2++]!=':'){}
	tmp =0;
	char ch1[20];	
	i = 0;
	while(to[i]!='\0'){
		if(to[i]=='/'){
			i++;
			ch1[tmp++]='\0';
			tmp = 0;			
		}
		ch1[tmp++]=to[i++];
	}			
	strcat(substr,  ch1);
	strcat(substr,  fileContent+pos2-1);
	fd = fopen(tmpPath,"w");	
	fprintf(fd, "%s", substr);
	fclose(fd);
	free(fileContent);
	free(substr);	
	free(lastPath);
	free(tmpPath);	
}

static void makeFile(const char *path){	
	char *p = strstr(path,"/.");	
	int p1 =p-path;
	if(p1>=0){printf("freturned\n");return;}
	int block = getOneBlock();
	setFreeBlocks(block);
	int blockfile = getOneBlock();	
	setFreeBlocks(blockfile);
	int rootblocknum = getThatBlock(path);
	if(block == -1){
		printf("file system filled up");
		return;
	}
	char str[5];
	sprintf(str, "%d", block);
	FILE *fd = NULL;
	time_t seconds = time(NULL);
	char *tmpPath=(char *) malloc(30);
	strcpy(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,str);
	fd = fopen(tmpPath,"w");
	fprintf(fd,"{size:0,uid:1,gid:1,mode:33261,linkcount:2,atime:%ld,ctime:%ld,mtime:%ld,indirect:0,location:%d}",seconds,seconds,seconds,blockfile);
	fclose(fd);	
	sprintf(str, "%d", rootblocknum);		
	strcpy(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,str);
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%[^\n]s", fileContent);
	fclose(fd);
	char *point = strstr (fileContent,"size:");
	int pos1 = point - fileContent + 5;
	int pos2 = pos1;
	while(fileContent[pos2++]!=','){}
	char* substr  = (char *) malloc(BLOCK_SIZE);	
	strncpy(substr, fileContent+pos1, pos2-pos1-1);	
	int sizeblock = atoi(substr);	
	point = strstr (fileContent,"linkcount:");
	pos1 = point - fileContent + 10;
	pos2 = pos1;
	while(fileContent[pos2++]!=','){}
	strncpy(substr, fileContent+pos1, pos2-pos1-1);	
	int links = atoi(substr);
	point = strstr (fileContent,"filename_to_inode_dict:{");
	pos1 = point - fileContent + 24;
	pos2 = pos1;
	while(fileContent[pos2++]!='}'){}
	strcpy(substr,"");
	strncat(substr, fileContent+pos1, pos2-pos1-1);
	char* lastPath  = (char *) malloc(20);
	int tmp =0;
	char ch[20];
	int i = 0;
	while(path[i]!='\0'){
		if(path[i]=='/'){
			i++;
			for(tmp = 0 ; tmp < 20 ; tmp++){
				ch[tmp] = '\0';
			}		
			tmp = 0;			
		}
		ch[tmp++]=path[i++];
	}	
	strcpy(lastPath,ch);
	strcat(substr,",f:");
	strcat(substr,lastPath);
	strcat(substr,":");
	sprintf(str, "%d", block);
	strcat(substr,str);	
	fd = fopen(tmpPath,"w");
	fprintf(fd, "{size:%d,uid:1000,gid:1000,mode:16877,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,filename_to_inode_dict:{%s}}",(sizeblock+1),seconds,seconds,seconds,(links+1),substr);
	fclose(fd);
	free(fileContent);
	free(substr);	
	free(lastPath);
	free(tmpPath);
}

static void writeToFile(const char *path){
	char *p = strstr(path,"/.goutputstream");	
	int p1 =p-path;
	if(p1>=0){printf("freturned\n");return;}
	printf("in write %s\n",buffer);
	int rootblocknum = getThatBlock(path);		
	printf("after rootblock\n");
	char str[5];
	sprintf(str, "%d", rootblocknum);	
	FILE *fd = NULL;
	char* lastPath  = (char *) malloc(20);	
	char *tmpPath=(char *) malloc(30);
	int tmp =2;
	char ch[20];
	ch[0] = 'f';
	ch[1] = ':';
	int i = 0;	
	while(path[i]!='\0'){
		if(path[i]=='/'){
			i++;
			for(tmp = 2 ; tmp < 20 ; tmp++){
				ch[tmp] = '\0';
			}
			tmp = 2;			
		}
		ch[tmp++]=path[i++];
	}		
	strcpy(lastPath,ch);	
	strcpy(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,str);
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%[^\n]s", fileContent);
	fclose(fd);	
	printf("%s lat %s\n", fileContent,lastPath);
	char* filenum  = (char *) malloc(5);	
	char *point = strstr (fileContent,lastPath);
	int pos1 = point - fileContent + strlen(lastPath)+1;
	int pos2 = pos1;
	while(1){
		if(fileContent[pos2]=='}' || fileContent[pos2]==','){
			break;
		}pos2++;
	}
	printf("after getting file block number\n");
	strcpy(filenum,"");
	strncat(filenum, fileContent+pos1, pos2-pos1);	
	printf("filenum %s\n",filenum);
	strcpy(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,filenum);
	printf("going to get size of buffer\n");
	int size = strlen(buffer);
	printf("size %d\n",size);
	int numFiles = size/BLOCK_SIZE +1;	
	printf("opeingn %s\n", tmpPath);
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%[^\n]s", fileContent);
	fclose(fd);
	printf("%s\n",fileContent );
	point = strstr (fileContent,"indirect:");
	pos1 = point - fileContent + 9;
	if(numFiles <= 1){
		fileContent[pos1] = '0';
	}else{
		fileContent[pos1] = '1';
	}	
	fd = fopen(tmpPath,"w");
	fprintf(fd, "%s", fileContent);
	fclose(fd);
	point = strstr (fileContent,"location:");
    pos1 = point - fileContent + 9;
	pos2 = pos1;
	while(fileContent[pos2++]!='}'){}
	strcpy(filenum,"");
	strncat(filenum, fileContent+pos1, pos2-pos1-1);	
	strcpy(tmpPath,"FileSysData/fusedata.");
	strcat(tmpPath,filenum);
	printf("buffer %s\n", buffer);		
	if(numFiles == 1){
		fd = fopen(tmpPath,"w");
		fprintf(fd, "%s", buffer);
		fclose(fd);
	}else{
		fd = fopen(tmpPath,"r");
		fscanf(fd, "%[^\n]s", fileContent);
		fclose(fd);
		int tmpnum = 0;
		i = 0;
		while(fileContent[i]!='\0'){
			if(fileContent[i]==','){
				tmpnum++;
			}i++;	
		}
		int tmplistofFile[numFiles];
		fd = fopen(tmpPath,"r");
		fscanf(fd, "%[^\n]s", fileContent);
		fclose(fd);		
		i = 1;
		pos1 = 0;
		for(tmp = 0 ; tmp < 5 ; tmp++){
			str[tmp] = '\0';
		}tmp = 0;
		while(fileContent[i]!='}'){
			if(fileContent[i]==','){
				i++;
				tmplistofFile[pos1++] = atoi(str);
				for(tmp = 0 ; tmp < 5 ; tmp++){
					str[tmp] = '\0';
				}tmp=0;
				if(fileContent[i]=='}'){break;}
			}
			str [tmp++] = fileContent[i++];
		}
		for(i =0 ; i < pos1 ; i++){	
			addFreeBlocks(tmplistofFile[i]);
		}

		while(tmpnum <= numFiles){
			char* substr  = (char *) malloc(BLOCK_SIZE);			
			strcpy(substr,"");
			strncat(substr, fileContent,strlen(fileContent)-1);
			int block = getOneBlock();
			setFreeBlocks(block);
			sprintf(str, "%d", block);
			strcat(substr, str);
			strcat(substr, ",}");			
			fd = fopen(tmpPath,"w");
			fprintf(fd, "%s", substr);
			fclose(fd);
			free(substr);
			tmpnum++;
		}
		int listofFile[numFiles];
		fd = fopen(tmpPath,"r");
		fscanf(fd, "%[^\n]s", fileContent);
		fclose(fd);		
		i = 1;
		pos1 = 0;
		for(tmp = 0 ; tmp < 5 ; tmp++){
			str[tmp] = '\0';
		}tmp = 0;
		while(fileContent[i]!='}'){
			if(fileContent[i]==','){
				i++;
				listofFile[pos1++] = atoi(str);
				for(tmp = 0 ; tmp < 5 ; tmp++){
					str[tmp] = '\0';
				}tmp=0;
				if(fileContent[i]=='}'){break;}
			}
			str [tmp++] = fileContent[i++];
		}		
		for(i =0 ; i < pos1 ; i++){	
			char *filePath=(char *) malloc(30);
			strcpy(filePath,"FileSysData/fusedata.");
			strcat(filePath,listofFile[i]);
			strncpy(fileContent,(buffer + (4096*i)),4096);
			fd = fopen(filePath,"w");
			fprintf(fd, "%s", fileContent);
			fclose(fd);	
			free(filePath);
			free(fileContent);			
		}				
	}
	free(fileContent);
	free(filenum);	
	free(lastPath);
	free(tmpPath);
	free(buffer);
}

static int amay_getattr(const char *path, struct stat *stbuf){
	int res;	
	if(path[0] == '/' && path[1] == 't' && path[2] == 'm' && path[3] == 'p'){
		printf("%s\n",path);
	}
	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;
	return 0;
}

static int amay_fgetattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi){
	int res;
	(void) path;
	res = fstat(fi->fh, stbuf);
	if (res == -1)
		return -errno;
	return 0;
}

struct amay_dirp {
	DIR *dp;
	struct dirent *entry;
	off_t offset;
};

static int amay_opendir(const char *path, struct fuse_file_info *fi){
	int res;
	struct amay_dirp *d = malloc(sizeof(struct amay_dirp));
	if (d == NULL)
		return -ENOMEM;
	d->dp = opendir(path);
	if (d->dp == NULL) {
		res = -errno;
		free(d);
		return res;
	}
	d->offset = 0;
	d->entry = NULL;
	fi->fh = (unsigned long) d;
	return 0;
}

static inline struct amay_dirp *get_dirp(struct fuse_file_info *fi){
	return (struct amay_dirp *) (uintptr_t) fi->fh;
}

static int amay_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi){
	struct amay_dirp *d = get_dirp(fi);
	(void) path;
	if (offset != d->offset) {
		seekdir(d->dp, offset);
		d->entry = NULL;
		d->offset = offset;
	}
	while (1) {
		struct stat st;
		off_t nextoff;

		if (!d->entry) {
			d->entry = readdir(d->dp);
			if (!d->entry)
				break;
		}
		memset(&st, 0, sizeof(st));
		st.st_ino = d->entry->d_ino;
		st.st_mode = d->entry->d_type << 12;
		nextoff = telldir(d->dp);
		if (filler(buf, d->entry->d_name, &st, nextoff))
			break;
		d->entry = NULL;
		d->offset = nextoff;
	}
	return 0;
}

static int amay_releasedir(const char *path, struct fuse_file_info *fi){
	struct amay_dirp *d = get_dirp(fi);
	(void) path;
	closedir(d->dp);
	free(d);
	return 0;
}

static int amay_mknod(const char *path, mode_t mode, dev_t rdev){
	int res;
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;
	return 0;
}

static int amay_mkdir(const char *path, mode_t mode){
	int res;
	if(path[0] == '/' && path[1] == 't' && path[2] == 'm' && path[3] == 'p'){
		printf("Making Dir %s\n",path);	
		makedir(path);
		res = mkdir(path, mode);
		if (res == -1)
			return -errno;
	}else{
		printf("==CANNOT DO THAT NOT OUTSIDE /tmp Dir\n");		
	}
	return 0;
}

static int amay_unlink(const char *path){
	int res;		
	res = unlink(path);	
	printf("unlinked\n");
	removeDir(path,1);	
	if (res == -1)
		return -errno;
	return 0;
}

static int amay_rmdir(const char *path){
	int res;
	if(path[0] == '/' && path[1] == 't' && path[2] == 'm' && path[3] == 'p'){
		printf("Removing Dir %s\n",path);	
		removeDir(path,0);
		res = rmdir(path);
		if (res == -1)
			return -errno;
	}else{
		printf("CANNOT DO THAT NOT OUTSIDE /tmp Dir\n");
		return 0;
	}
}

static int amay_rename(const char *from, const char *to){
	int res;
	printf("Renaming %s to %s\n",from,to);	
	if(write_buf_flag == 0){
		renameDir(from, to);	
	}else{
		writeToFile(to);
	}		
	res = rename(from, to);
	if (res == -1)
		return -errno;
	return 0;
}

static int amay_link(const char *from, const char *to){
	int res;
	res = link(from, to);
	if (res == -1)
		return -errno;
	return 0;
}

static int amay_truncate(const char *path, off_t size){
	int res;
	res = truncate(path, size);
	if (res == -1)
		return -errno;
	return 0;
}

static int amay_ftruncate(const char *path, off_t size,
			 struct fuse_file_info *fi){
	int res;
	(void) path;
	res = ftruncate(fi->fh, size);
	if (res == -1)
		return -errno;
	return 0;
}

static int amay_create(const char *path, mode_t mode, struct fuse_file_info *fi){	
	if(path[0] == '.'){
		printf("Don't put in dot(.)\n");	
		return;	
	}
	int fd;
	fd = open(path, fi->flags, mode);	
	makeFile(path);
	if (fd == -1)
		return -errno;
	fi->fh = fd;
	return 0;
}

static int amay_open(const char *path, struct fuse_file_info *fi){
	int fd;	
	fd = open(path, fi->flags);
	if (fd == -1)
		return -errno;
	fi->fh = fd;
	return 0;
}

static int amay_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi){
	int res;
	(void) path;
	res = pread(fi->fh, buf, size, offset);
	if (res == -1)
		res = -errno;
	return res;
}

static int amay_read_buf(const char *path, struct fuse_bufvec **bufp,
			size_t size, off_t offset, struct fuse_file_info *fi){
	struct fuse_bufvec *src;
	(void) path;
	src = malloc(sizeof(struct fuse_bufvec));
	if (src == NULL)
		return -ENOMEM;
	*src = FUSE_BUFVEC_INIT(size);
	src->buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
	src->buf[0].fd = fi->fh;
	src->buf[0].pos = offset;
	*bufp = src;
	return 0;
}

static int amay_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi){
	int res;
	(void) path;	
	res = pwrite(fi->fh, buf, size, offset);
	printf("in %s\n", buf);
	buffer = (char *) malloc(strlen(buf));	
	strcpy(buffer,buf);	
	printf("going to write %s",buf);	
	writeToFile(path);	
	write_buf_flag=1;
	if (res == -1)
		res = -errno;
	return res;
}

static int amay_statfs(const char *path, struct statvfs *stbuf){
	int res;
	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;
	return 0;
}

static int amay_release(const char *path, struct fuse_file_info *fi){
	(void) path;
	close(fi->fh);
	return 0;
}

static void superblock(){
	FILE *fd = NULL;
	time_t seconds = time(NULL);
	fd = fopen("FileSysData/fusedata.0","w");
	fprintf(fd,"{creationTime:%ld,mounted:50,devId:20,freeStart:1,freeEnd:25,root:26,maxBlocks:10000}",seconds);
	fclose(fd);
}

static void rootblock(int link){
	FILE *fd = NULL;
	time_t seconds = time(NULL);
	fd = fopen("FileSysData/fusedata.26","w");
	fprintf(fd,"{size:%d,uid:1000,gid:1000,mode:16877,atime:%ld,ctime:%ld,mtime:%ld,linkcount:%d,filename_to_inode_dict:{d:.:26,d:..:26}}",size,seconds,seconds,seconds,link);
	fclose(fd);
}

static void freeBlocks(){
	FILE *fd = NULL;
	char *addedPath = "/fusedata.";
	int i =0, j = 1;
	char str[5];
	fd = fopen("FileSysData/fusedata.1","w");
	fprintf(fd,"{");
	fclose(fd);
	fd = fopen("FileSysData/fusedata.1","a");
	char *tmpPath=(char *) malloc(30);
	for(i = 27; i < MAX_BLOCKS ;i++){
		if(i % 400 == 0){
			j++;
			strcpy(tmpPath,"FileSysData");
			sprintf(str, "%d", j);
			strcat(tmpPath,addedPath);
			char *filename = strcat(tmpPath,str);
			fprintf(fd,"}");
			fclose(fd);			
			fd = fopen(filename,"w");
			fprintf(fd,"{");
			fclose(fd);
			fd = fopen(filename,"a");
		}
		fprintf(fd,"%d,",i);		
	}
	fprintf(fd,"}");
	fclose(fd);
}

static void initialize(){	
			superblock();
			freeBlocks();
			rootblock(2);
}

static void amay_init(){
	system("mkdir -p FileSysData");
	char *buff = (char *) malloc(512);
	memset(buff,'0',512);
	FILE *fd = NULL;
	fd = fopen("FileSysData/fusedata.0","r");
	if(fd == NULL){
		int i=0,j=0;
		char *addedPath = "/fusedata.";
		char str[5];
		char *tmpPath=(char *) malloc(30);
		for(i = 0; i < MAX_BLOCKS ;i++){
			strcpy(tmpPath,"FileSysData");
			sprintf(str, "%d", i);
			strcat(tmpPath,addedPath);
			char *filename = strcat(tmpPath,str);
			fd = fopen(filename,"w");
			for(j = 0; j < 8;j++){ // to write 4096 0's i.e. 512 bytes 8 times.
				fprintf(fd,"%s",buff);
			}
			fclose(fd);
		}
		initialize();
	}	
	getFreeBlocks();
	free(buff);		
}

static void  MakeFiles(const char *path, int block){
	FILE *fd = NULL;
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	char str[5];
	sprintf(str, "%d", block);
	char *tmpPath=(char *) malloc(30);
	strcpy(tmpPath, "FileSysData/fusedata.");
	strcat(tmpPath, str);
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%[^\n]s", fileContent);
	fclose(fd);	
	char *point = strstr (fileContent,"indirect:");
	int pos1 = point - fileContent + 9;
	char ch = fileContent[pos1];
	char *substr = (char *) malloc(10);	
	point = strstr (fileContent,"location:");
    pos1 = point - fileContent + 9;
	int pos2 = pos1;	
	while(fileContent[pos2++]!='}'){}
	strcpy(substr, "");
	strncat(substr, fileContent+pos1,pos2-pos1-1);	
	strcpy(tmpPath, "FileSysData/fusedata.");
	strcat(tmpPath, substr);
	strcpy(fileContent,"");	
	fd = fopen(tmpPath,"r");
	fscanf(fd,"%[^\n]s", fileContent);
	fclose(fd);
	if(ch  == '0'){
		fd = fopen(path,"w");		
		fprintf(fd, "%s", fileContent);
		fclose(fd);
	}else{

		int numFiles = 0;
		pos2 =0;
		while(fileContent[pos2]!='}'){if(fileContent[pos2]==','){numFiles++;}pos2++;}		
		int listofFile[numFiles];	
		int tmp = 0;
		pos1 = 0;
		for(tmp = 0 ; tmp < 5 ; tmp++){
			str[tmp] = '\0';
		}tmp = 0;
		int i = 1 ;
		while(fileContent[i]!='}'){						
			if(fileContent[i]==','){
				i++;
				;	
				listofFile[pos1++] = atoi(str);

				for(tmp = 0 ; tmp < 5 ; tmp++){
					str[tmp] = '\0';
				}tmp=0;
				if(fileContent[i]=='}'){break;}
			}
			str [tmp++] = fileContent[i++];
		}
		for(i = 0 ; i < pos1 ; i++){				
			strcpy(tmpPath, "FileSysData/fusedata.");
			sprintf(str, "%d", listofFile[i]);
			strcat(tmpPath, str);	
			fd = fopen(tmpPath,"r");
			fscanf(fd,"%[^\n]s", fileContent);
			fclose(fd);
			fd = fopen(path,"a");
			fprintf(fd, "%s", fileContent);
			fclose(fd);
		}
	}	
	free(fileContent);
	free(tmpPath);
	free(substr);
}

static void  LoadFS(const char *path, int block){	
	FILE *fd = NULL;
	char *fileContent = (char *) malloc(BLOCK_SIZE);	
	char str[5];
	sprintf(str, "%d", block);
	char *tmpPath=(char *) malloc(30);
	strcpy(tmpPath, "FileSysData/fusedata.");
	strcat(tmpPath, str);
	fd = fopen(tmpPath,"r");
	fscanf(fd, "%[^\n]s", fileContent);
	fclose(fd);	
	int tmpComma = 0;
	char *point = strstr(fileContent,"filename_to_inode_dict:{");
	int pos1 = point - fileContent;
	while(fileContent[pos1]!='}' && tmpComma < 2){
		if(fileContent[pos1]==','){ 
			tmpComma++;
		} pos1++;
	}	
	int pos2 = point - fileContent;
	if(fileContent[pos1]=='}'){return;}
	pos1--;	
	char *command = (char *) malloc(512);
	char *paths = (char *) malloc(512);
	char *tmp = (char *) malloc(30);		
	strcpy(paths,path);
	printf("paths %s\n", paths);
	while(fileContent[pos1]!='}'){
		if(fileContent[pos1] == ','){			
			if(fileContent[pos1+1] == 'd'){
				pos2 = pos1+3;				
				while(fileContent[pos2++]!=':'){}
				strcpy(tmp,"");
				strncat(tmp,fileContent+pos1+3 , pos2-pos1-4);
				pos1 = pos2;
				while(1){					
					if(fileContent[pos2]==',' || fileContent[pos2]=='}'){break;}pos2++;
				}
				strncpy(str,fileContent+pos1 , pos2-pos1);
				block = atoi(str);				
				strcat(paths,"/");
				strcat(paths,tmp);				
				strcpy(command,"mkdir -p \"");
				strcat(command,paths);
				strcat(command,"\"");
				system(command);
				LoadFS(paths,block);				
				pos1 = pos2;								
			}else if(fileContent[pos1+1] == 'f'){				
				pos2 = pos1+3;
				while(fileContent[pos2++]!=':'){}
				strcpy(tmp,"");
				strncat(tmp,fileContent+pos1+3 , pos2-pos1-4);
				pos1 = pos2;				
				while(1){					
					if(fileContent[pos2]==',' || fileContent[pos2]=='}'){break;}pos2++;
				}				
				strncpy(str,fileContent+pos1 , pos2-pos1);
				block = atoi(str);	
				strcpy(command,path);
				strcat(command,"/");
				strcat(command,tmp);					
				MakeFiles(command, block);				
				pos1 = pos2;
			}
		}
	}
	free(fileContent);
	free(paths);
	free(tmp);
	free(command);

}

static struct fuse_operations amay_oper = {
	.getattr	= amay_getattr,
	.fgetattr	= amay_fgetattr,
	.opendir	= amay_opendir,
	.readdir	= amay_readdir,
	.releasedir	= amay_releasedir,
	.mknod		= amay_mknod,
	.mkdir		= amay_mkdir,
	.unlink		= amay_unlink,
	.rmdir		= amay_rmdir,
	.rename		= amay_rename,
	.link		= amay_link,
	.truncate	= amay_truncate,
	.ftruncate	= amay_ftruncate,
	.create		= amay_create,
	.open		= amay_open,
	.read		= amay_read,
	.read_buf	= amay_read_buf,
	.write		= amay_write,	
	.statfs		= amay_statfs,
	.release	= amay_release,
	.flag_nullpath_ok = 0,       
};

int main(int argc, char *argv[]){
	amay_init();
	system("rm -rf /tmp/*");
	LoadFS("/tmp",26);				
	if(argc != 1){
		printf("Usage : ./AmayFuse\n");
		return -1;
	}
	system("rm -rf Amay");
	system("mkdir -p Amay");
	argv[argc++] = "Amay";
	//argv[argc++] = "-f"; // For debugging purposes
	umask(0);
	return fuse_main(argc, argv, &amay_oper, NULL);
}