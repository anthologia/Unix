// "fio_bin_fwrite.c"를 여러번 include한 경우 한번만 include하기 위해
#ifndef _FILE_IO_C
#define _FILE_IO_C

//*****************************************************************************
//   FILE 열고 닫기 함수들:  C 표준 fopen() fclose() fsetpos()
//*****************************************************************************

static FILE *fp;

int r_open_fp(char *name) { // 파일 불러오기 : 읽기용으로 열기
	return( (fp = fopen(name, "r")) ? 0 : -1 );
	// 위 문장은 return ( (fp = fopen(name, "r")) != NULL ? 0 : -1); 와 동일함
	// NULL DMS (void *)0, 즉 포인터 값(주소 값)이 0임
 	// FILE *P = NULL; 이것은 p = (void *)0과 같고, p에는 사실 0이 저장됨
	// if (p)하면, 이 조건문은 p가 0이므로 FALSE임
  	// 만약 p가 NULL이 아니면 TRUE임
}

// 아래 함수들은 FILE I/O type 0, 1, 2에서 모두 사용됨
int w_open_fp(char *name) {
	return( (fp = fopen(name, "w")) ? 0 : -1 );
}

int n_open_fp(char *name) { // 다른 (새) 이름으로 저장하기 : 쓰기용으로 열기
	//먼저 읽기용으로 파일 열기
	if((fp = fopen(name, "r"))) {
	fclose(fp);
	return (-2);
	}
	return( (fp = fopen(name, "w")) ? 0 : -1 );
}

int rw_open_fp(char *name) { // 파일을 읽기/쓰기용으로 열기 (파일 없으면 에러)
						 // 파일 레코드 직접 조작 메뉴 선택 시 호출됨
	return( (fp = fopen(name, "r+")) ? 0 : -1 );
}

int rwc_open_fp(char *name) { // 파일을 읽기, 쓰기용으로 열기 (파일 없으면 생성)
	return((fp = fopen(name, "w+")) ? 0 : -1);
}

//	DB 파일을 닫는다.
int close_fp(void) { // fp 파일 닫기
	return(fclose(fp));
}

int setpos_fp(int pos) { // fp 파일의 읽고 쓰는 위치(file position)을 pos로 옮김
	return(fseek(fp, pos, SEEK_SET)); // SEEK_SET, SEEK_END, SEEK_CUR
}

//===========================================================================
// TEXT FILE I/O 관련 함수들 : C 표준 fscanf() fprintf()
//===========================================================================

static void
trunc_space(char *p)
{
	while (*p) ++p;
	for (--p; isspace((int)*p); --p) ;
	*(++p) = 0;
}

//	DB 파일에서 레코드 하나를 읽어 메모리 r에 저장한다.
int
fscanf_rec(rec_t *r) {
	if(fscanf(fp, "%d %s %s %d", &r->key, r->name, r->dep, &r->grade) <= 0)
		return 0;

	fgetc(fp); // 주소 앞에 있는 ' ' 읽어 들임
			   // 주소는 "%s"로 읽어 들일 수 없음(단어가 여러 개임)

	if (fgets(r->addr, 40, fp) == NULL)
		return 0; // 실제 길이 40임 : %-38s + '\n' + '\0'

	trunc_space(r->addr); // 주소 뒤의 ' ' 삭제
	return (sizeof(rec_t));
}

//	DB 파일에 레코드 r을 저장한다.
int
fprintf_rec(rec_t *r)
{
	return(fprintf(fp, "%-10d %-11s %-15s %1d %-38s\n", \
		r->key, r->name, r->dep, r->grade, r->addr));
}

//	DB 파일에서 각각의 헤드정보를 읽어 해당 변수에 저장한다
int
fscanf_hd(head_t *h) {
	// scanf()처럼 입력을 받되 키보드가 아닌
	// 파일fp에서 입력 받음
	return(fscanf(fp, "%s %d %d %s %f %d %d %d\n",
		h->name, &h->head_sz, &h->rec_sz, h->program, \
		&h->version, &h->start_key, &h->rec_num, &h->fio_type));
	// %s는 구조체 멤버 배열이름, 그 외 %d %f는 멤버의 주소
}

//	DB 파일에 각각의 헤드정보 변수 값을 저장한다.
int
fprintf_hd(head_t *h) {
	// printf() 처럼 출력하되 화면이 아니라 파일 fp에 출력
	return(fprintf(fp, "%-19s %-2d %-2d %-19s %-3.1f %-6d %-6d %1d\n", \
		h->name, h->head_sz, h->rec_sz, h->program, \
		h->version, h->start_key, h->rec_num, h->fio_type));
}

//===========================================================================
// TEXT FILE I/O 관련 함수들 : C 표준 sscanf() sprintf() fgets() fputs()
//===========================================================================

static char sbuf[BUF_LEN];

//	DB 파일에서 레코드 하나를 읽어 메모리 r에 저장한다.
int
sscanf_rec(rec_t *r) {
	// fp 파일에서 한 줄(행), 즉 “\n”까지 읽어 들여 메모리 sbuf[]에 저장
	if (fgets(sbuf, BUF_LEN, fp) == NULL)
		return(0); // 에러 발생
				   // fscanf_rec() 함수를 참조
	sscanf(sbuf, "%d %s %s %d", &r->key, r->name, r->dep, &r->grade);
	strcpy(r->addr, sbuf + 41); // 주소는 단어가 여러 개라 별도로 복사함; %s불가능
	trunc_space(r->addr);
	return(sizeof(rec_t));
}

//	DB 파일에 레코드 r을 저장한다.
int
sprintf_rec(rec_t *r) {
	// fprintf_rec() 함수를 참조
	// fprintf()처럼 출력하되, 파일이 아니라 메모리 sbuf[]에 출력
	sprintf(sbuf, "%-10d %-11s %-15s %1d %-38s\n", \
		r->key, r->name, r->dep, r->grade, r->addr);
	return(fputs(sbuf, fp)); // sbuf[]의 문자열을 파일 fp에 씀
}

//	DB 파일에서 각각의 헤드정보를 읽어 해당 변수에 저장한다.
int
sscanf_hd(head_t *h) {
	// fp 파일에서 한 줄(행), 즉 “\n”까지 읽어 들여 메모리 sbuf[]에 저장한다.
	if (fgets(sbuf, BUF_LEN, fp) == NULL)
		return(0); // 에러 발생
				   // fscanf_hd() 함수의 fscanf()를 참조한다.
	return(sscanf(sbuf, "%s %d %d %s %f %d %d %d\n",
		h->name, &h->head_sz, &h->rec_sz, h->program, \
		&h->version, &h->start_key, &h->rec_num, &h->fio_type));
	// fscanf()처럼 입력을 받되 파일이 아니라 메모리 sbuf[]에서 입력을 받음
}

//	DB 파일에 각각의 헤드정보 변수 값을 저장한다.
int
sprintf_hd(head_t *h) {
	// fprintf_hd() 함수의 fprintf()를 참조
	// fprintf()처럼 출력하되, 파일이 아니라 메모리 sbuf[]에 출력
	sprintf(sbuf, "%-19s %-2d %-2d %-19s %-3.1f %-6d %-6d %1d\n", \
		h->name, h->head_sz, h->rec_sz, h->program, \
		h->version, h->start_key, h->rec_num, h->fio_type);
	return(fputs(sbuf, fp)); // sbuf[]의 문자열을 파일 fp에 씀
}

//===========================================================================
// Binary FILE I/O 관련 함수들 : C 표준 fread() fwrite()
//===========================================================================

//	DB 파일에서 레코드 하나를 읽어 메모리 r에 저장한다.
int
fread_rec(rec_t *r) {
	//fwrite(파일에서 읽어온 데이터 저장할 메모리 주소, 단위원소의크기, 원소의개수, fp);
	return(fread(&r, sizeof(rec_t), 1, fp));
}

//	DB 파일에 레코드 r을 저장한다.
int
fwrite_rec(rec_t *r) {
	//fwrite(파일에 쓸 메모리 주소, 단위원소의크기, 원소의개수, fp);
	return(fwrite(&r, sizeof(rec_t), 1, fp));
}

//	DB 파일에서 각각의 헤드정보를 읽어 해당 변수에 저장한다.
int
fread_hd(head_t *h) {
	// fwrite()한 순서대로 멤버들을 읽어 들어야 함
	//fread(파일에서 읽어온 데이터 저장할 메모리 주소, 단위원소의크기, 원소의개수, fp);
	fread(h->name, sizeof(char), NAME_LEN, fp);
	fread(&h->head_sz, sizeof(int), 1, fp);
	fread(&h->rec_sz, sizeof(int), 1, fp);
	fread(h->program, sizeof(char), NAME_LEN, fp);
	fread(&h->version, sizeof(float), 1, fp);
	fread(&h->start_key, sizeof(int), 1, fp);
	fread(&h->rec_num, sizeof(int), 1, fp);
	fread(&h->fio_type, sizeof(int), 1, fp);
	// 나머지 멤버들도 이런 식으로 작성할 것
	return(sizeof(head_t)); // 에러가 발생하지 않았다고 가정하고 헤드 크기 리턴

}

//	DB 파일에 각각의 헤드정보 변수 값을 저장한다.
int
fwrite_hd(head_t *h) {
	// char 배열의 이름은 주소임, int와 float 멤버는 주소를 직접 지정해 주어야 함
	//fwrite(파일에 쓸 메모리 주소, 단위원소의크기, 원소의개수, fp);
	fwrite(h->name, sizeof(char), NAME_LEN, fp);
	fwrite(&h->head_sz, sizeof(int), 1, fp);
	fwrite(&h->rec_sz, sizeof(int), 1, fp);
	fwrite(h->program, sizeof(char), NAME_LEN, fp);
	fwrite(&h->version, sizeof(float), 1, fp);
	fwrite(&h->start_key, sizeof(int), 1, fp);
	fwrite(&h->rec_num, sizeof(int), 1, fp);
	fwrite(&h->fio_type, sizeof(int), 1, fp);
	// 나머지 멤버들도 이런 식으로 작성할 것
	return(sizeof(head_t)); // 에러가 발생하지 않았다고 가정하고 헤드 크기 리턴
}


//===========================================================================
// Binary FILE I/O 관련 함수들 : UNIX API open() closde() read() write()
//===========================================================================

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int fd;

int
r_open_fd(char *name) {	// 파일 불러오기 : 읽기용으로 열기
	return((fd = open(name, O_RDONLY)));
}

int
w_open_fd(char *name) { // 저장하기: 파일을 쓰기용으로 열기
	return((fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644)));
}

int
n_open_fd(char *name) { // 다른(새) 이름으로 저장하기:
						// 쓰기용, 파일이 있으면 에러, 없으면 새로 생성
	return((fd = open(name, O_WRONLY | O_CREAT | O_EXCL, 0644)));
}

int
rw_open_fd(char *name) { // 파일을 읽기/쓰기용으로 열기 (파일 없으면 에러)
						 // 파일 레코드 직접 조작 메뉴 선택 시 호출됨
	return((fd = open(name, O_RDWR)));
}

int
rwc_open_fd(char *name) { // 파일을 읽기/쓰기용으로 열기 (파일 없으면 생성)
	return((fd = open(name, O_RDWR | O_CREAT, 0644)));
}

//	DB 파일을 닫는다.
int
close_fd(void) { // fd 파일 단기
	return(close(fd));
}

//	DB 파일에서 레코드 하나를 읽어 메모리 r에 저장한다.
int
read_rec(rec_t *r) {
	//read(파일기술자, 파일에서 읽어 온 데이터 저장할 메모리 주소, 메모리 크기);
	return(read(fd, r, sizeof(rec_t)));
}

//	DB 파일에 레코드 r을 저장한다.
int
write_rec(rec_t *r) {
	//write(파일기술자, 파일에 쓸 메모리 주소, 메모리 크기(파일 쓸 데이터 길이));
	return(write(fd, r, sizeof(rec_t)));
}

//	DB 파일에서 각각의 헤드정보를 읽어 해당 변수에 저장한다.
int
read_hd(head_t *h) { // write()한 순서대로 멤버들을 읽어 들어야 함
	//read(fd, 파일에서 읽어 온 데이터 저장할 메모리 주소, 메모리 크기);
	read(fd, h->name, NAME_LEN);
	read(fd, &h->head_sz, sizeof(int));
	read(fd, &h->rec_sz, sizeof(int));
	read(fd, h->program, NAME_LEN);
	read(fd, &h->version, sizeof(float));
	read(fd, &h->start_key, sizeof(int));
	read(fd, &h->rec_num, sizeof(int));
	read(fd, &h->fio_type, sizeof(int));

	return(sizeof(head_t)); // 에러가 발생하지 않았다고 가정하고 헤드 크기 리턴
}

//	DB 파일에 각각의 헤드정보 변수 값을 저장한다.
int
write_hd(head_t *h) { // 파일에 헤드 h를 저장한다
	// char 배열의 이름은 주소임, int와 float 멤버는 주소를 직접 지정해 주어야 함
	//write(fd, 파일에 쓸 메모리 주소, 메모리 크기(파일 쓸 데이터 길이));
	write(fd, h->name, NAME_LEN);
	write(fd, &h->head_sz, sizeof(int));
	write(fd, &h->rec_sz, sizeof(int));
	write(fd, h->program, NAME_LEN);
	write(fd, &h->version, sizeof(float));
	write(fd, &h->start_key, sizeof(int));
	write(fd, &h->rec_num, sizeof(int));
	write(fd, &h->fio_type, sizeof(int));

	return(sizeof(head_t));
}

int 
setpos_fd(int pos) { // fd 파일의 읽고 쓰는 위치(file position)를 pos로 옮김
	return(lseek(fd, SEEK_SET, pos)); // SEEK_SET, SEEK_END, SEEK_CUR
}

#endif  /* _FILE_IO_C */

