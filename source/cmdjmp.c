#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <setjmp.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>
#include <time.h>

#define SZ_STR_BUF		256	// 일반 문자열 배열 길이
#define SZ_FILE_BUF		1024	//FILE I/O BUF SIZE

// get_argv_optv() 함수에서 상세히 설명함
char *cmd;
char *argv[100];
char *optv[10];
int  argc, optc;
char cur_work_dir[SZ_STR_BUF];  //현재 작업 디렉토리 이름을 저장하는 버퍼
jmp_buf jump;
int cmd_idx;


// 에러 원인을 화면에 출력하고 이 매크로를 사용하는 해당 함수에서 리턴함
/* cmd가 "ls"라면 "ls: 에러원인" 형태로 출력됨 */  
#define EQUAL(_s1, _s2) 	(strcmp(_s1, _s2) == 0) // 두개 문자열 같으면 true
#define NOT_EQUAL(_s1, _s2)	(strcmp(_s1, _s2) != 0)	// 두개 문자열 다르면 true


// 명령어 사용법을 출력함
// help()와 proc_cmd()에서 호출됨
static void print_usage(char *msg, char *cmd, char *opt, char *arg) {
	// proc_cmd()에서 msg로 "사용법: "을, 
	// help()    에서 msg로 "    "  을 넘겨 줌
	printf("%s%s", msg, cmd);	// "사용법: ls"
	if (NOT_EQUAL(opt, "")) 	// 옵션이 있으면
		printf("  %s", opt);	// "-l  "
	printf("  %s\n", arg);		// "[디렉토리 이름]"
	// 최종 출력 예) "사용법: ls  -l  [디렉토리 이름]\n"
}

//***************************************************************************
//  명령어 옵션 및 인자 개수가 정확한지 체크하는 작업을 수행함
//***************************************************************************

// [명령어 인자의 개수]가 정확한지 체크함
// argc: 사용자가 입력한 명령어 인자의 수
// count: 그 명령어가 필요로 하는 인자의 수
// 리턴값: 인자개수가 정확하면 0, 틀렸으면 -1
static void check_arg(int count) {
	if(count < 0) {
		count = -count;
		if(argc <= count)
			return;
	}
	if (argc == count) {
		return;
	}
	
	if (argc > count) {
		printf("불필요한 명령어 인자가 있습니다.\n");
	} else {
		printf("명령어 인자의 수가 부족합니다.\n");
	}
	longjmp(jump, -2);
}

// 명령어에서 정확한 [옵션]이 주어졌는지 체크함
// optc: 사용자가 입력한 옵션 개수
// optv[i]: 사용자가 입력한 옵션 문자열
// opt: 그 명령어가 필요로 하는 옵션 문자열
// 리턴값: 옵션이 없으면 0, 있으면 1, 옵션이 틀렸으면 -1
static void check_opt(char *opt) {
	int i, err = 0;
	for (i = 0; i < optc; ++i) {
		if (NOT_EQUAL(opt, optv[i])) {
			printf("지원되지 않는 명령어 옵션(%s)입니다.\n", optv[i]);
			err = -1;
		}
	}
	if (err) longjmp(jump, -2);
}

//***************************************************************************
// 	get_argv_optv(): 명령어 한줄을 명령어,옵션,명령인자를 각 단어별로 분리
//***************************************************************************
//
// 전역변수
// char *cmd; 		// 명령어 문자열
// char *argv[100];	// 명령어 인자 문자열들 시작주소 저장
// char *optv[10];	// 명령어 옵션 문자열들 시작주소 저장
// int optc; // 옵션 토큰 수, 즉, optv[]에 저장된 원소의 개수
// int argc; // 명령어와 옵션을 제외한 명령어 인자의 수
//			 // 즉, argv[]에 저장된 원소의 개수

// 입력된 명령어 행 전체를 저장하고 있는 cmd_line[]에서
// 각 토큰(단어)을 별개의 문자열로 자른 후, 그 토큰의 시작 주소를 
// 명령어일 경우 cmd에, 옵션일 경우 optv[]에, 
// 명령어 인자인 경우 argv[]에 각각 순서적으로 저장한다.

// 예제: get_argv_optv(cmd_line)를 호출하여 리턴한 후의 각 변수의 값
//
// cmd_line[]에 "ls -l pr4" 가 저장되어 있을 경우
// cmd -> "ls"
// optc = 1, optv[0] -> "-l";
// argc = 1, argv[0] -> "pr4";

// cmd_line[]에 "ln -s f1 f2" 가 저장되어 있을 경우
// cmd -> "ln"
// optc = 1, optv[0] -> "-s";
// argc = 2, argv[0] -> "f1", argv[1] -> "f1";

// cmd_line[]에 "ln f1 f2" 가 저장되어 있을 경우
// cmd -> "ln"
// optc = 0;
// argc = 2, argv[0] -> "f1", argv[1] -> "f1";

// cmd_line[]에 "pwd" 가 저장되어 있을 경우
// cmd -> "pwd"
// optc = 0;
// argc = 0;

// cmd, optv[], argv[]에 저장되는 것은 각 문자열의 시작주소, 
// 즉, 각 문자열의 첫 글자의 주소가 저장됨
// 따라서 각 문자열은 여전히 cmd_line[]에 저장되어 있음

static char * get_argv_optv(char *cmd_line) {
	//  사용자가 키보드에서 입력한 명령어 한 줄이 cmd_line[]에 저장되어 있음
	//  cmd_line[]에 "ln -s file1 file2"가 저장되어 있다고 가정하자.
	//
	//  strtok()의 두번째 인자인 " \t\n"는 토큰(단어)을 구분하는 구분자임.
	//	즉, 스페이스 ' ', 탭 '\t', 엔터 '\n' 글자가 나오면 토큰을 자름.
	//  strtok() 함수는 cmd_line[]에 있는 문자열(전체가 하나의 문자열)에서
	// 	첫 단어 "ln"를 찾아 단어의 끝에 null문자('\0')를 삽입해 
	// 	단어를 하나의 문자열로 만들고(단어를 자른다고 표현함), 
	//	그 문자열의 첫 글자 주소를 리턴함.
	//  첫 단어를 잘라 내기 위해 strtok() 호출 시 첫 인자로 cmd_line을 주고, 
	//	다음에 또 호출할 땐 첫 인자로 NULL을 주는데,
	//	이 경우 앞에서 처리한 단어의 그 다음 단어를 찾아 자른다.
	// 	만약 함수 호출 시 더 이상 처리할 단어가 없다면 NULL을 리턴함.
	//
	//  결국 함수 호출 [후]에는 cmd_line[]가 "ln\0-s\0file1\0file2"로 변함
	//	'\0'는 null문자(한 글자임)를 표현한 것이고,
	//	메모리에는 ASCI 코드 0(정수값)이 한 byte로 저장되어 있음. 
	//	모든 문자열 끝에는 이 문자가 있음(함수들이 자동으로 삽입해 줌)
	//  즉, cmd_line[]에는 네개의 문자열이 저장되어 있고,
	//	각 문자열의 시작 주소는 optv[]와 argv[]에 분리되어 저장되어 있음
	char* tok;
	argc = optc = 0;

	if((cmd = strtok(cmd_line, " \t\n")) == NULL)
		return (NULL);

	for( ; (tok = strtok(NULL, " \t\n")) != NULL; ) {
		if(tok[0] == '-') {
			optv[optc++] = tok;
		} else {
			argv[argc++] = tok;
		}
	}
	return (cmd);
}

//***************************************************************************
// ls() 함수에서 호출되는 함수들 시작

// "ls -l" 옵션 준 경우 하나의 파일에 대한 상세정보 출력하기
static void print_attr(char* path, char* fn) {
	struct passwd *pwp;
	struct group *grp;
	struct stat st_buf;
	char full_path[SZ_STR_BUF], buf[SZ_STR_BUF], c;
	char time_buf[13];
	struct tm *tmp;

	sprintf(full_path, "%s/%s", path, fn);
	if (lstat(full_path, &st_buf) < 0 ) {
		longjmp(jump, -1);
	}
	
	if (S_ISREG(st_buf.st_mode))	c = '-';
	else if (S_ISDIR(st_buf.st_mode))	c = 'd';
	else if (S_ISCHR(st_buf.st_mode))	c = 'c';
	else if (S_ISBLK(st_buf.st_mode))	c = 'b';
	else if (S_ISFIFO(st_buf.st_mode))	c = 'f';
	else if (S_ISLNK(st_buf.st_mode))	c = 'l';
	else if (S_ISSOCK(st_buf.st_mode))	c = 's';
	
	buf[0] = c;
	buf[1] = (st_buf.st_mode & S_IRUSR) ? 'r' : '-';
	buf[2] = (st_buf.st_mode & S_IWUSR) ? 'w' : '-';
	buf[3] = (st_buf.st_mode & S_IXUSR) ? 'x' : '-';
	buf[4] = (st_buf.st_mode & S_IRGRP) ? 'r' : '-';
	buf[5] = (st_buf.st_mode & S_IWGRP) ? 'w' : '-';
	buf[6] = (st_buf.st_mode & S_IXGRP) ? 'x' : '-';
	buf[7] = (st_buf.st_mode & S_IROTH) ? 'r' : '-';
	buf[8] = (st_buf.st_mode & S_IWOTH) ? 'w' : '-';
	buf[9] = (st_buf.st_mode & S_IXOTH) ? 'x' : '-';
	buf[10] = '\0';
	printf("%s", buf);
	
	pwp = getpwuid(st_buf.st_uid);
	grp = getgrgid(st_buf.st_gid);
	tmp = localtime(&st_buf.st_mtime);
	strftime(time_buf, 13, "%b %d %H:%M", tmp);
	sprintf(buf+10, " %3ld %-8s %-8s %8ld %s %s", st_buf.st_nlink, pwp->pw_name, grp->gr_name, st_buf.st_size, time_buf, fn);

	if(S_ISLNK(st_buf.st_mode)) {
		int len, bytes;
		strcat(buf, " -> ");
		len = strlen(buf);
		bytes = readlink(full_path, buf+len, SZ_STR_BUF-len);
		buf[len+bytes] = '\0';
	}
	printf("%s\n", buf);
}


// "ls -l" 옵션 준 경우 : 디렉토리의 모든 파일에 대해 상세정보 출력하기
static void print_detail(DIR *dp, char *path) {
	struct dirent *dirp;

	while ((dirp = readdir(dp)) != NULL) {
		print_attr(path, dirp->d_name);
	}
	closedir(dp);
}


// "ls"만 한 경우 : 가장 긴 파일이름의 길이를 계산한 후 이를 기준으로 한 줄에 출력할 수 있는 열(column)의 개수를 결정
static void get_max_name_len(DIR *dp, int *p_max_name_len, int *p_num_per_line) {
	struct dirent *dirp;
	int max_name_len = 0;

	//모든 파일 이름을 읽어 가장 긴 이름의 길이를 결정함
	while ((dirp = readdir(dp)) != NULL) {
		int name_len = strlen(dirp->d_name);
		if (name_len > max_name_len) {
			max_name_len = name_len;
		}
	}
	// 디렉토리 읽는 위치 처음으로 되돌리기
	rewinddir(dp);
	
	// 가장 긴 파일이름 + 이름 뒤의 여유 공간
	max_name_len += 4;
	
	// 한 줄에 출력할 파일이름의 개수 결정
	*p_num_per_line = 80 / max_name_len;
	*p_max_name_len = max_name_len;
}

	
// "ls"만 한 경우 : 디렉토리의 모든 파일에 대해 이름만 출력하기
static void print_name(DIR *dp) {
	struct dirent *dirp;
	int max_name_len, num_per_line, cnt = 0;

	// max_name_len : 가장 긴 파일이름 길이 +4 (이름 뒤의 여유공간)
	// num_per_line : 한 줄에 출력할 수 있는 총 파일이름의 개수

	get_max_name_len(dp, &max_name_len, &num_per_line);
	while ((dirp = readdir(dp)) != NULL) {
		printf("%-*s", max_name_len, dirp->d_name);

		// cnt : 현재까지 한 줄에 출력한 파일이름 개수
		// 한 줄에 출력할 수 있는 파일이름의 개수만큼
		// 이미 출력했으면 줄 바꾸기
		if((++cnt % num_per_line) == 0) {
			printf("\n");
		}
	}
	
	// 마지막 줄에 줄 바꾸기 문자 출력
	// 앞에서 이미 한 줄에 출력할 수 있는 파일이름의 개수만큼 이미 출력하여
	// 줄 바꾸기를 했으면 또 다시 줄 바꾸기를 하지 않음
	if ((cnt % num_per_line) != 0) {
		printf("\n");
	}
}
// 
// ls() 함수에서 호출되는 함수들 끝
//***************************************************************************


//***************************************************************************
//
// 	여기서부터 각 명령어 구현 시작

void cat(void) {
	int fd, len;
	char buf[SZ_FILE_BUF];

	if ((fd = open(argv[0], O_RDONLY)) < 0) {
		longjmp(jump , -1);
	}

	while ((len = read(fd, buf, SZ_FILE_BUF)) > 0) {
		if(write(STDOUT_FILENO, buf, len) != len) {
			len = -1;
			break;
		}
	}

	if (len < 0) {
		perror(cmd);
	}
	close(fd);
}


// 현재 작업 디렉토리를 변경하는 명령어
// 사용법 : cd [디렉토리 이름]
//			[]는 명령어 인자를 주어도 되고 안 주어도 됨을 의미함
// argv[0] -> "디렉토리 이름"; [디렉토리 이름]을 준 경우
// argc = [디렉토리 이름]을 준 경우 1, 주지 않았을 경우 0
void cd(void) {
	struct passwd *pwp = getpwuid(getuid());

	if(argc==0) {
		argv[0] = pwp->pw_dir;
	}
	if(chdir(argv[0]) < 0) {
		longjmp(jump, -1);
	} else {
		getcwd(cur_work_dir, SZ_STR_BUF);
	}
}


void changemod(void) {
	int mode;
	sscanf(argv[0], "%o", &mode);

	if(chmod(argv[1], mode) < 0) {
		longjmp(jump, -1);
	}
}


// 파일을 다른 이름으로 복사하는 명령어
// 사용법: cp  원본파일이름  복사된파일이름
// argv[0] -> "원본파일이름"
// argv[1] -> "복사된파일이름"
void cp(void) {
	int rfd, wfd, len;
	char buf[SZ_FILE_BUF];
	struct stat st_buf;

	if((stat(argv[0], &st_buf) < 0) || ((rfd = open(argv[0], O_RDONLY)) < 0)) {
			longjmp(jump, -1);
	}
	
	if((wfd = creat(argv[1], st_buf.st_mode)) < 0) {
		perror(cmd); 	//PRINT_ERR_RET() 사용하지 않는 이유는 PRINT_ERR_RET() 사용 시에는
		close(rfd);  	//ERROR 호출 후, return 하기에 close를 할 수 없다.
		return;		//그렇기때문에 perror(cmd)를 사용하는 것이다.
	}
		
	while((len = read(rfd, buf, SZ_FILE_BUF)) > 0) {
		if(write(wfd, buf, len) != len) {
			len = -1;
			break;
		}
	}
	
	if(len < 0) {
		perror(cmd);
	}
	close(rfd);
	close(wfd);
}


void date(void) {
	char stm[128];
	time_t ttm = time(NULL);
	struct tm *ltm = localtime(&ttm);

	printf("atm: %s", asctime(ltm));
	printf("ctm: %s", ctime(&ttm));
	strftime(stm, 128, "stm: %a %b %d %H:%M%S %Y", ltm);
	puts(stm);
}


// echo 다음에 입력된 문자열을 화면에 바로 echo해 주는 명령어
// 사용법: echo [echo할 문자열 입력: 길이 제한 없음]
//   예) echo I love you. 
// argv[0] -> "I"
// argv[1] -> "love"
// argv[2] -> "you."
// argc = 3; 에코할 문장의 총 토큰(단어)의 수
void echo(void) {
	int i;

	for (i = 0; i < argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
}


void hostname(void) {
	char hostname[SZ_STR_BUF];

	hostname[SZ_STR_BUF] = gethostname(hostname, SZ_STR_BUF);
	printf("%s ", hostname);
	printf("\n");
}


void id(void) {
	struct passwd *pwp;
	struct group *grp;

	pwp = (argc==1) ? getpwnam(argv[0]) : getpwuid(getuid());
	if((pwp == NULL) || ((grp = getgrgid(pwp->pw_gid)) == NULL)) {
		printf("%s : 잘못된 사용자 이름 : \"%s\"\n", cmd, argv[0]);
	} else {
		printf("uid=%ld(%s) gid=%ld(%s)\n", pwp->pw_uid, pwp->pw_name, grp->gr_gid, grp->gr_name);
	}
}


void ln(void) {
	int ret;

	if(((optc==1) ? symlink(argv[0], argv[1]) : link(argv[0], argv[1])) < 0) {
		longjmp(jump, -1);
	}
}


// 디렉토리 내의 파일 이름을 보여 주는 명령어
// 사용법: ls [-l] [디렉토리 이름]
//		"-l" 옵션이 없으면 파일 이름만, 있으면 파일의 상세정보를 보여 줌
//		[디렉토리 이름]이 없으면 현재 디렉토리를 의미함
// argv[0] -> "디렉토리이름"; [디렉토리 이름]을 준 경우
// argc = [디렉토리 이름]을 준 경우 1, 안 준 경우 0
// optc = "-l" 옵션을 준 경우 1, 주지 않았을 경우 0
void ls(void) {
	char *path;
	DIR *dp;

	//디렉토리 이름을 주지 않았다면 현재 디렉토리 설정
	path = (argc == 0) ? "." : argv[0];

	//디렉토리 열기
	if((dp = opendir(path)) == NULL) {
		longjmp(jump, -1);
	}
	
	if(optc == 0) {
		print_name(dp);
	} else {
		print_detail(dp, path);
	}
	
	//디렉토리 닫기
	closedir(dp);
}
	

void makedir(void) {
	if(mkdir(argv[0], 0755) < 0) {
		longjmp(jump, -1);
	}
	
	// 0755 : rwxr-xr-x
	// 이는 이 디렉토리를 만든 사람은 이 디렉토리에
	// 읽고, 쓰고, 옮겨 갈 수 있음
	// 그러나 그룹멤버나 다른 3자는
	// cd명령어로 옮겨가서 ls만 할 수 있고
	// 그 디렉토리에 파일을 생성하거나 삭제는 할 수 없다.
}	


void mv(void) {
	if((link(argv[0], argv[1]) < 0) || (unlink(argv[0]) < 0)) {
		longjmp(jump, -1);
	}
}


void pwd(void) {
	printf("%s ", cur_work_dir);
	printf("\n");
}


// 하나의 파일을 삭제하는 명령어
// 사용법: rm  파일이름
// argv[0] -> "파일이름"
void rm(void) {
	struct stat buf;
	int ret;
	char *name = strtok(NULL, " \t\n");


	if((lstat(argv[0], &buf) < 0) || (((S_ISDIR(buf.st_mode)) ? rmdir(argv[0]) : unlink(argv[0])) < 0)) {
		longjmp(jump, -1);
	}

	/*
	if(S_ISDIR(buf.st_mode))
		ret = rmdir(argv[0]);
	else
		ret = unlink(argv[0]);
	if(ret < 0)
		longjmp(jump, -1);	

	
	if(lstat(argv[1], &buf) < 0)
		longjmp(jump, -1);	
	if(((S_ISDIR(buf.st_mode) ? rmdir(argv[0]) : unlink(argv[0]) < 0)
		longjmp(jump, -1);
	*/
}


// 프로그램을 종료하는 명령어
// 사용법: exit
void quit(void) {
	exit(0);
}


// optc = "-a" 옵션 주면 1, 안 주면 0
void unixname(void) {
	struct utsname un; //시스템 정보를 저장하는 구조체 변수
	
	uname(&un);
	printf("%s ", un.sysname);

	if(optc) {
		printf("%s %s %s %s", un.nodename, un.release, un.version, un.machine);
	}
	printf("\n");
}


void removedir(void) {
	if(rmdir(argv[0]) < 0) {
		longjmp(jump, -1);
	}
}


void touch(void) {
	int fd;
	
	if(utime(argv[0], NULL) == 0) {
		return ;
	} else if((errno != ENOENT) || ((fd = creat(argv[0], 0644)) < 0 )) {
		longjmp(jump, -1);
	}
	
	close(fd);
}


void Sleep(void) { //sleep()은 기존 라이브러리에 존재하므로 Sleep()으로 구현한다.
	int sec;
	sscanf(argv[0], "%d", &sec);
	sleep(sec);
}


void whoami(void) {
	struct passwd *pwp;
	pwp = getpwuid(getuid());
	printf("%s\n", pwp->pw_name);

	// char* username;
	// username = getlogin();
	// if(username != NULL)
	// 	printf("%s", username);
	// else
	//	printf("터미널 장치가 아니라 사용자 계정정보를 구할 수 없습니다.\n");
}


// cmd_tbl[]의 각 배열 원소에서 
// 명령어 처리함수 이름을 사용하기 때문에 이 함수들은 아래 cmd_tbl[] 보다 
// 먼저 함수정의가 되어야 하고 실제 앞에 이미 정의 되었다.
// 그러나 help() 함수의 정의는 cmd_tbl[] 뒤에 있기 때문에, cmd_tbl[] 앞에 
// 이 함수를 아래처럼 미리 선언해야 한다. help() 함수정의는 cmd_tbl[]을 
// 직접 사용하기 때문에 어쩔 수 없이 cmd_tbl[] 뒤에 있어야 한다.
void help(void);


//***************************************************************************
//  각 명령어별 상세 정보를 저장한 구조체 배열 cmd_tbl[]
//***************************************************************************

#define AC_LESS_1		-1		// 명령어 인자 개수가 0 또는 1인 경우
#define AC_ANY			-100	// 명령어 인자 개수가 제한 없는 경우(echo)

// 하나의 명령어에 대한 정보 구조체
typedef struct {
	char *cmd;			// 명령어 문자열
	void (*func)(void);		// 명령어를 처리하는 함수 시작 주소(함수 이름)
	int  argc;			// 명령어 인자의 개수(명령어와 옵션은 제외)
	char *opt;			// 옵션 문자열: 예) "-a", 옵션 없으면 ""
	char *arg;			// 명령어 사용법 출력할 때 사용할 명령어 인자: 
} cmd_tbl_t;			//		예) cp인 경우 "원본파일 복사파일""

cmd_tbl_t cmd_tbl[] = {
	{ "cat",		cat,		1,			"",		"파일이름" },
	{ "cd",			cd,			AC_LESS_1,	"",		"[디렉토리이름]"},
	{ "chmod",		changemod,	2,			"",		"8진수mode 파일이름"},
	{ "cp",			cp,			2,			"",		"원본파일  복사된파일" },
	{ "date",		date,		0,			"",		"" },	
	{ "echo",		echo,		AC_ANY,		"",		"[에코할 문장]" },
	{ "help",		help,		0,			"",		"" },
	{ "hostname",	hostname,	0,			"",		"" },
	{ "id",			id,			AC_LESS_1,	"",		"[사용자계정]" },
	{ "ln",			ln,			2,			"-s",	"원본파일 링크파일"},
	{ "ls",			ls,			AC_LESS_1,	"-l",	"[디렉토리이름]" },
	{ "exit",		quit,		0,			"",		"" },
	{ "mkdir",		makedir,	1,			"",		"디렉토리" },
	{ "mv",			mv,			2,			"",		"원본파일  바뀐이름"},
	{ "pwd",		pwd,		0,			"",		"" }, 
	{ "rm",			rm,			1,			"",		"파일이름" },
	{ "touch",		touch,		1,			"",		"파일이름" },
	{ "rmdir",		removedir,	1,			"",		"디렉토리" },
	{ "uname",		unixname,	0,			"",		"-a"},
	{ "whoami",		whoami,		0,			"",		"" },
};
	
// (총 명령어 개수) = (cmd_tbl[] 배열 전체 크기) / (배열의 한 원소 크기)
int num_cmd = sizeof(cmd_tbl) / sizeof(cmd_tbl[0]);


// 이 프로그램이 지원하는 명령어 리스트를 보여 주는 명령어
// 사용법: help
// 이 함수가 일반 명령어를 처리하는 함수들과는 달리 여기에 배치된 것은
// 바로 위에 있는 cmd_tbl[]을 이 함수가 직접 사용하기 때문이다.
void help(void) {
	int  k;

	printf("현재 지원되는 명령어 종류입니다.\n");
	for (k = 0; k < num_cmd; ++k) {
		print_usage("  ", cmd_tbl[k].cmd, cmd_tbl[k].opt, cmd_tbl[k].arg);
	}
	printf("\n");
}

	
// run_cmd() :
// 외부 명령어를 실행해 주는 함수
// cmd 자신을 복제하여 부모 프로레스는 자식이 종료할 때까지 대기하고,
// 자식 프로세스는 외부 명령어 프로그램으로 대체하여 실행함
void run_cmd(void) {
	pid_t pid;

	// fork()를 호출하여 기존에 실행 중인 cmd를 복제하여 새로운 프로세스 생성.
	// 에러 발생 시, 에러 원인 출력 후 리턴.

	if(pid = fork() < 0 ) {
		longjmp(jump, -1);
	}

	// 이후로 동일한 프로그램(cmd) 2개가 동시에 실행되고 있으며,
	// 프로그램 내에서 실행되는 위치도 바로 이 위치에서 실행된다.
	// fork()의 리턴값 (pid)이 0이면 자식 프로세스이고, 아니면 부모 프로세스.
	
	else if (pid == 0) {
		int i, cnt = 0;
		char *nargv[100];

		// 기존의 명령어 cmd, 옵션 optv[], 인자 argv[]에 저장된 포인터 값을
		// 순서대로 nargv[]에 연속적으로 저장함; nargv[]의 마지막은 NULL.
		nargv[cnt++] = cmd; 			// 기존의 cmd를 nargv[]에 저장
		for(i = 0; i < optc; ++i) {
			nargv[cnt++] = optv[i]; 	//기존의 optv[]를 nargv[]에 저장
		}
		
		for(i = 0; i < argc; ++i) {
			nargv[cnt++] = argv[i]; 	// 기존의 argv[]를 nargv[]에 저장
		}
		nargv[cnt++] = NULL;			//nargv[]의 마지막은 NULL이여야 함

		if(execvp(cmd, nargv) < 0) {
			// 해당 프로그램이나 접근권한이 없거나 실행 프로그램이 아닐 때 에러
			perror(cmd); 	// 여기서는 PRINT_ERR_RET() 사용 X
			exit(1); 	// 에러 발생 시 자식 프로세스는 종료해야함
		}

	// 자식 프로세스는 외부 프로그램으로 대체되었기에 이후의 코드는 실행 X
	// fork()의 리턴값(pid)이 양수(pid = 자식 프로세스의 ID)이면 부모 프로세스
	// 부모 프로세스(cmd 프로그램)는 자식 프로세스가 종료될 때까지
	// 더 이상 실행되지 않고 여기서 대기해야한다.
	// 이 과정에서 에러 발생 시, 에러 원인 출력 후 스스로 종료한다.
	} else { 
		//pid > 0, 즉 부모 프로세스
		// 자식(pid)이 종료될 때까지 대기
		if(waitpid(pid, NULL, 0) < 0) {
			longjmp(jump, -1);	
		}
		// 자식 프로세스가 종료되면 waitpid()는 자식 프로세스의 pid값을 리턴.
	}
	// 여기서 부모 프로세스가 자식 프로세스의 종료를 기다리지 않으면,
	// 자식 프로세스는 Zombie 프로세스가 된다.
	// 그러므로 꼭 waitpid()를 사용하자
}

	
// proc_cmd():
// 입력된 명령어와 동일한 이름의 명령어 구조체를 cmd_tbl[] 배열 찾아 낸 후
// 명령어 인자와 옵션을 제대로 입력했는지 체크하고 잘못 되었으면 사용법을
// 출력하고 정상이면 해당 명령어를 처리하는 함수를 호출하여 실행함
void proc_cmd(void) {
	int  k;

	// 입력한 명령어 정보를 cmd_tbl[]에서 찾음
	for (k = 0; k < num_cmd; ++k) {
		
		// 전역변수char*cmd: 사용자가 입력한 명령어 문자열 주소, 예)cmd->"ls"
		if (EQUAL(cmd, cmd_tbl[k].cmd)) { // 입력 명령어를 cmd_tbl에서 찾았음
			cmd_idx = k;
			check_arg(cmd_tbl[k].argc);
			check_opt(cmd_tbl[k].opt);
			cmd_tbl[k].func();

			return ;
			/*
			if ((check_arg(cmd_tbl[k].argc) < 0) || 	// 인자 개수 체크
				(check_opt(cmd_tbl[k].opt)  < 0))		// 옵션 체크
				// 인자 개수 또는 옵션이 잘못 되었음: 사용법 출력
				print_usage("사용법: ", cmd_tbl[k].cmd,
									    cmd_tbl[k].opt, cmd_tbl[k].arg);
			else
				cmd_tbl[k].func();		// 명령어를 처리하는 함수를 호출

			return;
			*/
		}
	}
	// 사용자가 입력한 명령어를 cmd_tbl[]에서 찾지 못한 경우
	run_cmd();
}
	

int main(int argc, char *argv[]) {
	char cmd_line[SZ_STR_BUF]; 	// 입력된 명령어 라인 전체를 저장 공간
	int cmd_count = 1; 		//현재 명령어 번호
	int jmpret;

	setbuf(stdout, NULL);		// 표준 출력 버퍼 제거: printf() 즉시 화면 출력
	setbuf(stderr, NULL);		// 표준 에러 출력 버퍼 제거

	help();				// 명령어 전체 리스트 출력
	getcwd(cur_work_dir, SZ_STR_BUF); //현재 작업 디렉토리 이름을 구해와 cur_wok_dir[]에 저장

	if((jmpret = setjmp(jump)) != 0) {
		if(jmpret = -1) {
			perror(cmd);
		} else if(jmpret = -2) {
			print_usage("사용법: ", cmd_tbl[cmd_idx].cmd, cmd_tbl[cmd_idx].opt, cmd_tbl[cmd_idx].arg);
		}
	}

	for (;;) {
		// 명령 프롬프트 출력: "<현재작업디렉토리> 명령어번호: "
		printf("<%s> %d: ",cur_work_dir, cmd_count ); 
		gets(cmd_line);	// 키보드에서 한 행을 입력 받아 cmd_line[]에 저장

		// 입력받은 한 행(cmd_line[])의 명령어, 옵션, 인자를 개별 문자열로 분리
		if (get_argv_optv(cmd_line) != NULL) {
			cmd_count++;
			proc_cmd();	
		}
		// else 명령어는 주지 않고 엔터만 친 경우 명령어 번호는 증가하지 않음
	}
}
