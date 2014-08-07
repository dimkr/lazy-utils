#include <fcntl.h>
int main(int argc, char **argv){
	int fds[argc-1],len,i,mask=O_WRONLY|O_CREAT;
	char buf[4096];
	++argv;
	if (*argv[0]=='-' && *argv[1]=='a'){
		mask|=O_APPEND;
		--argc;
		++argv;
	}
	fds[0]=1;
	for(i=1;i<argc;)fds[i++]=open(*argv++,mask);
	while (len=read(0,buf,4096)){
		for(i=0;i<argc;)
			write(fds[i++],buf,len);
	}
	for(i=1;i<argc;)close(fds[i++]);
	return 0;
}
