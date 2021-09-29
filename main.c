#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <memory.h>
#include <math.h>
#include <time.h>
#include <windows.h>
#include <mysql.h>
#pragma comment (lib, "libmysql.lib")
#define DB_HOST "127.0.0.1"					// mysql hostname
#define DB_USER "root"						// mysql user name
#define DB_PASS "cwal03664!@#"				// mysql password
#define DB_NAME "score"						// mysql db
#define CHOP(x) x[strlen(x) - 1] = ' '		// insert할때 문자열 chopping
#define FALSE 0					// 거짓
#define TRUE 1					// 참
#define MAP_X_MAX 96			// 최대 X 좌표
#define MAP_Y_MAX 32			// 최대 Y 좌표
#define FLOOR_Y 26				// 바닥 Y 좌표
#define OBJECT_MAX 32			// 생성될 수 있는 Object의 최댓값
#define SPAWN_TIME 15000		// mobs의 생성 주기 (15000ms)

// user 구조체 선언
typedef struct _Character {
	short coord[2], size[2];	// user의 x, y 좌표값 및 크기
	float accel[2], flyTime;	// user의 가속값(0: x, 1: y) 및 체공 시간
	bool direction;				// user의 방향, true면 오른쪽, false면 왼쪽

	// user status
	char name[16];							// user 이름. 아래 score와 함께 저장됨.
	int lv, exp[2], score, hp[2], mp[2];	// exp[] -> 0=exp required, 1=current exp // hp[], mp[] -> 0=max value, 1=current value
	short power, weapon;					// user의 공격력, 무기

	// animation control
	// 0=leg_motion, 1,2,3=attack_motion(1, 2, 3), invincibility motion
	short motion[4];

	// 0=hp, mp 회복 tick, 1=이동 움직임 tick, 2=공격 움직임 tick, 3=대쉬 움직임 tick, 4=피격 움직임 tick
	unsigned int tick[6];
} Character;

// object(items, mobs, particles) 구조체 선언
typedef struct _Object {		// enemies, projectiles, particles, etc.
	short coord[2], size[2];	// object의 x, y 좌표값 및 크기
	float accel[2], flyTime;		// object의 가속값 및 체공 시간
	bool direction;				// object의 방향, true면 오른쪽, false면 왼쪽

	short kind;		// 1~99: items, 100~199: mobs, 200~: particles
	int hp[2], exp;	// hp: this value is used randomly for item or particle object
	short dam;

	short motion[3];		// motion
	unsigned int tick[4];	// 0: hpshow time(enemy) or active time(projecticles, particles)
} Object;

// user 구조체 초기화
Character character = {
	{MAP_X_MAX / 2, MAP_Y_MAX / 2}, {3, 3},		// x,y 맵 정중앙, 크기 3 by 3
	{0, 0},	0,
	1,												// direction
	"",												// name
	1, {100, 0}, 0, {100, 100}, {50, 50},			// lv.1, hp 100, mp 50
	10, 0,											// power: 10
	{0, 1, 0, 0},									// motion
	{0, 0, 0, 0, 0}									// tick
};
Object** objects;

unsigned int tick = 0;											// 게임 경과 시간
unsigned int spon_tick = 0;										// 몬스터 스폰 주기
char sprite_floor[MAP_X_MAX];									// 바닥 배열
char mapData[MAP_X_MAX * (MAP_Y_MAX + 1)];						// array for graphics

const short stat_enemy[2][7] ={			// mobs의 hp, exp, size(x y), tick (2 3 4)
	{150, 30, 4, 3, 0, 1000, 0},
	{300, 50, 5, 5, 0, 500, 0}
};
const short stat_weapon[3] = { 5, 10, 15 };
const char sprite_character[10] = " 0  | _^_";			// user character 3 * 3
const char sprite_character_leg[2][3][4] = {
	{"-^.", "_^\'", "_^."},								// 왼쪽 move시 user leg 움직임 ( 3 motions )
	{".^-", "\'^_", ".^_"}								// 오른쪽 move시 user leg 움직임 ( 3 motions )
};
const char sprite_normalAttack[2][3][16] = {
	{" .- o          ", " .   (   o \'   ", "         o \'-  "},		// 왼쪽 3연타 시 user attack 움직임 ( 3 motions )
	{"o -.           ", "   . o   )   \' ", "     o      -\' "}			// 오른쪽 3연타 시 user attack 움직임 ( 3 motions )
};
const char sprite_weapon[2][3][4] = {
	{"---", "--+", "<=+"},											// 왼쪽 방향 무기 sprite
	{"---", "+--", "+=>"}											// 오른쪽 방향 무기 sprite
};
const char sprite_invenWeapon[3][11] = { "   /   /  ", "   /  '*. ", "  |   \"+\" " };			// weapon창에 들어갈 무기 sprite
const char sprite_enemy1[2][13] = { " __ (  )----", " __ [  ]\'--\'" };							// mob sprite

/*
	함수 정의
*/
void StartGame();					// 게임 시작
void UpdateGame();					// 게임 정보 업데이트
void ExitGame();					// 게임 종료
void SetConsole();					// 콘솔 창 설정
void ControlUI();					// UI 정보 업데이트
void ControlCharacter();			// user 정보 업데이트
void ControlObject();				// mobs, items, particles 정보 업데이트
void ControlItem(int index);		// item 정보 업데이트
void ControlEnemy(int index);		// mob 정보 업데이트
void ControlParticle(int index);	// particle 정보 업데이트
void CreateObject(short x, short y, short kind);			// mobs, items, particles 생성
void RemoveObject(int index);								// mobs, items, particles 제거
bool EnemyPosition(short x, short size_x);					// user를 바라보고있는 mobs의 방향
bool CollisionCheck(short coord1[], short coord2[], short size1[], short size2[]);		// user <-> objects 충돌 검사
void MoveControl(short coord[], float accel[], short size[], float* flyTime);			// motion Control
void DrawBox(short x, short y, short size_x, short size_y);								// size_x, size_y 크기의 상자를 x, y 좌표에 draw
void DrawNumber(short x, short y, int num);												// x, y 좌표에 number를 draw ( 왼쪽 정렬 )
void DrawSprite(short x, short y, short size_x, short size_y, const char spr[]);		// size_x, size_y 크기의 sprite를 x, y 좌표에 draw
void FillMap(char str[], char str_s, int max_value);											// 배열 초기화
void EditMap(short x, short y, char str);														// mapData에서 x, y 좌표의 내용 수정
int NumLen(int num);																				// 숫자 길이 return
void gotoxy(int x, int y);																			// 콘솔 커서 위치 이동
int menu();				// 시작 메뉴
void ViewScore();		// DB와 연동하여 스코어 표시
void InputScore();		// DB와 연동하여 스코어 DB에 입력

int main() {
	StartGame();			// main 함수 실행 시 각종 설정, 초기화 수행을 위해

	while (TRUE) {
		if (tick + 30 < GetTickCount64()) {				// 시간이 30ms 흘렀을때
			tick = GetTickCount64();					// 현재 시간을 tick으로

			UpdateGame();								// 게임 정보 update

			if (tick == 0)								// update 도중 tick이 0이 되면 break
				break;
		}
	}

	ExitGame();											// tick이 0일 경우, 게임 종료
	return 0;
}

void gotoxy(int x, int y) {
	COORD Pos = { x, y };		// 이 Pos 값을 x, y로
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), Pos);		// 콘솔의 커서를 Pos로 이동
}

// 메뉴 출력 함수
int menu() {
	// 좌표를 이동하며 메뉴를 출력
	gotoxy(35, 8);

	printf("Monster Hunter!");
	gotoxy(35, 10);
	printf("1. Play Game\n");
	gotoxy(35, 12);
	printf("2. View Score\n");
	gotoxy(35, 14);
	printf("3. How To Play\n");
	gotoxy(35, 16);
	printf("4. Exit\n");
	gotoxy(35, 18);
	printf(">>> ");

	// 메뉴에서 입력받은 값을 return
	int re;
	scanf("%d", &re);
	rewind(stdin);

	return re;
	
	system("cls");
}

// Score 표시 함수
void ViewScore() {
	MYSQL* connection = NULL, conn;		// connection: mysql connection, conn: mysql init
	MYSQL_RES* sql_result;				// query에 대한 result
	MYSQL_ROW sql_row;					// result를 row로 fetch한 배열

	int query_stat = 0;					// query의 결과 (성공 or 실패)

	mysql_init(&conn);		// mysql initialization

	mysql_options(&conn, MYSQL_SET_CHARSET_NAME, "euckr");			// mysql charset_name을 euckr로
	mysql_options(&conn, MYSQL_INIT_COMMAND, "SET NAMES euckr");	// mysql init_command를 euckr로

	// mysql connection 부분
	connection = mysql_real_connect(&conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 3306, (char*)NULL, 0);
	
	// connection 실패
	if (connection == NULL) {
		fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
		return 1;
	}

	// select 쿼리 전달
	query_stat = mysql_query(connection, "select * from scoretable order by score desc limit 10");
	
	// query stat이 0이 아닐 경우 -> query 전달 과정에서 에러 발생
	if (query_stat != 0) {
		fprintf(stderr, "Mysql query error : %s", mysql_error(&conn));
		return 1;
	}

	// query result 저장
	sql_result = mysql_store_result(connection);
	system("cls");
	gotoxy(38, 8);
	printf("Top 10 Users!\n");
	gotoxy(25, 10);
	printf("┌──────────────────────┬───────────────┐\n");
	int i = 11;
	// sql_result에서 row를 뽑아서 sql_row에 저장하는데 그 값이 NULL이 아닐때 까지 -> 모든 result 값에 대한 row를 출력
	while ((sql_row = mysql_fetch_row(sql_result)) != NULL) {
		gotoxy(25, i);
		printf("│\t%s\t\t│\t%s\t│\n", sql_row[0], sql_row[1]);		// sql_row[0]: name, sql_row[1]: score
		i++;
	}
	gotoxy(25, i);
	printf("└──────────────────────┴───────────────┘\n");

	// sql_result free
	mysql_free_result(sql_result);

	// mysql connection close
	mysql_close(connection);
}

// score 저장 함수
void InputScore() {
	MYSQL* connection = NULL, conn;		// connection: mysql connection, conn: mysql init

	int query_stat = 0;					// query의 결과 (성공 or 실패)
	char query[255];					// 전달할 query

	mysql_init(&conn);		// mysql initialization

	mysql_options(&conn, MYSQL_SET_CHARSET_NAME, "euckr");			// mysql charset_name을 euckr로
	mysql_options(&conn, MYSQL_INIT_COMMAND, "SET NAMES euckr");	// mysql init_command를 euckr로

	// mysql connection 부분
	connection = mysql_real_connect(&conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 3306, (char*)NULL, 0);

	// connection 실패
	if (connection == NULL) {
		fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
		return 1;
	}

	// name chopping을 위해 heap 할당
	char* name = malloc(sizeof(char) * strlen(character.name));
	CHOP(name);

	// query를 전달할 query 변수에 insert문 저장
	sprintf(query, "insert into scoretable values " "('%s', '%d')", character.name, character.score);
	// mysql의 connection에 query를 전달
	query_stat = mysql_query(connection, query);
	// 전달된 query에 에러 발생 시
	if (query_stat != 0) {
		fprintf(stderr, "Mysql query error : %s", mysql_error(&conn));
		return 1;
	}

	// 할당받은 변수 free
	free(name);
	// mysql connection close
	mysql_close(connection);
}

void StartGame() {
	SetConsole();								// console 설정 ( 커서 안보이게, title )
	srand((unsigned int)time(NULL));			// rand 함수를 위한 srand

	int ch = 0;

	while (1) {
		ch = menu();
		// play game
		if (ch == 1) {
			break;
		}
		// view score
		else if (ch == 2) {
			ViewScore();		// db에 저장된 데이터 표시
			_getch();
			system("cls");
		}
		// how to play
		else if (ch == 3) {
			system("cls");
			gotoxy(35, 10);
			printf("←, →:\t이동");
			gotoxy(35, 12);
			printf("↑:\t점프");
			gotoxy(35, 14);
			printf("z:\t공격");
			gotoxy(35, 16);
			printf("x:\t대쉬");
			gotoxy(35, 18);
			_getch();
			system("cls");
		}
		// etc
		else {
			exit(1);
		}
	}

	system("cls");
	gotoxy(32, 13);
	printf("Enter your name: ");
	scanf("%[^\n]s", character.name);		// 이름을 character 구조체에 저장
	rewind(stdin);

	gotoxy(0, 0);									// console 커서를 0, 0으로 이동

	FillMap(sprite_floor, '=', MAP_X_MAX);	// 바닥 배열 초기화

	objects = (Object**)malloc(sizeof(Object*) * OBJECT_MAX);		// objects 를 Object크기의 32만큼 할당 -> 32개의 object 저장 가능
	memset(objects, 0, sizeof(Object*) * OBJECT_MAX);				// 초기화
}

void UpdateGame() {
	FillMap(mapData, ' ', MAP_X_MAX * MAP_Y_MAX);	// mapData의 내용을 공백으로 초기화

	ControlCharacter();	// mapData(character) 를 업데이트
	ControlObject();		// mapData(enemy, projecticles, particles, etc...) 를 업데이트
	ControlUI();				// mapData(UI) 를 업데이트

	if (spon_tick + SPAWN_TIME < tick) {		// 스폰 주기가 지나면 
		spon_tick = tick;								// 현재 시간을 스폰 주기로 재설정
		CreateObject(rand() % 90, 10, 100);		// mob 소환
		CreateObject(rand() % 90, 10, 100);		// mob 소환
		CreateObject(rand() % 90, 10, 100);		// mob 소환
	}

	printf("%s", mapData);	// console에 mapData Print
}

void ExitGame() {
	for (int i = 0; i < OBJECT_MAX; i++) {
		if (objects[i])
			free(objects[i]);			// free가 되지 않은 object 구조체를 free
	}

	free(objects);					// objects 구조체를 free

	system("cls");
	gotoxy(35, 8);
	printf("Game Over!");
	InputScore();					// db에 데이터 저장

	gotoxy(35, 10);
	printf("Save Success!\n");
	_getch();
}

void SetConsole() {
	system("mode con:cols=96 lines=32");			// 행 96, 열 32 크기
	system("title Monster Hunter");								// title 설정

	HANDLE hConsole;
	CONSOLE_CURSOR_INFO ConsoleCursor;

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	ConsoleCursor.bVisible = 0;							// 커서 안보이게
	ConsoleCursor.dwSize = 1;							// 커서 두께 1

	SetConsoleCursorInfo(hConsole, &ConsoleCursor);		// console cursor info를 setting
}

// mapData에 UI 정보를 input하는 함수
void ControlUI() {
	int expPer = roundf(character.exp[1] * 100 / character.exp[0]);			// exp 현재 %
	int len;			// 이전 sprite의 길이

	DrawSprite(1, FLOOR_Y, MAP_X_MAX, 1, sprite_floor);	// 바닥 draw

	DrawBox(1, 1, 35, 8);				// UI box draw 35 * 8

	// inven의 무기 draw
	//  ex)
	// 	Weapon
	//  .-----.
	//  |    / |
	//  |  /   |
	//  '-----'
	//
	DrawBox(27, 4, 7, 4);				// box draw 7 * 4
	DrawSprite(28, 5, 5, 2, sprite_invenWeapon[character.weapon]);		// weapon sprite draw
	DrawSprite(28, 3, 6, 1, "Weapon");			// "Weapon"문자 draw

	// 이름, 레벨, hp, mp, 경험치 draw
	// ex)
	// 2 "test" LV.1 (0%)													SCORE: 0
	// 3 
	// 4  HP: 100 / 100
	// 5  MP: 50 / 50
	// 6
	// 7  Power: 10

	// draw name, lv, exp
	EditMap(3, 2, '\"');		// "
	DrawSprite(4, 2, strlen(character.name), 1, character.name);	len = 4 + strlen(character.name);		// test
	DrawSprite(len, 2, 7, 1, "\" LV.");	len += 5;								// " LV.
	DrawNumber(len, 2, character.lv);	len += NumLen(character.lv);		// 1
	DrawSprite(len, 2, 2, 1, " (");	len += 2;										//  (
	if (!expPer) {									// exp가 0일경우
		EditMap(len, 2, '0');	len++;		// 0
	}
	else {											// 아니면
		DrawNumber(len, 2, expPer);	len += NumLen(expPer);			// expPer draw
	}
	DrawSprite(len, 2, 2, 1, "%)");			// %)

	// draw score
	DrawSprite(MAP_X_MAX - NumLen(character.score) - 7, 2, 6, 1, "SCORE:");
	DrawNumber(MAP_X_MAX - NumLen(character.score), 2, character.score);

	// draw HP
	DrawSprite(4, 4, 3, 1, "HP:");	
	DrawNumber(8, 4, character.hp[1]);
	EditMap(9 + NumLen(character.hp[1]), 4, '/');
	DrawNumber(11 + NumLen(character.hp[1]), 4, character.hp[0]);

	// draw MP
	DrawSprite(4, 5, 3, 1, "MP:");
	DrawNumber(8, 5, character.mp[1]);
	EditMap(9 + NumLen(character.mp[1]), 5, '/');
	DrawNumber(11 + NumLen(character.mp[1]), 5, character.mp[0]);

	// draw power
	DrawSprite(4, 7, 6, 1, "Power:");
	DrawNumber(11, 7, character.power);
}

// mapData에 user의 움직임, 공격모션을 input하는 함수
void ControlCharacter() {
	// 이동, 공격
	bool move = FALSE, attack = FALSE;
	// 캐릭터 x, y 좌표
	int x = character.coord[0], y = character.coord[1];

	// 레벨업
	if (character.exp[1] >= character.exp[0]) {
		character.hp[0] += 10; character.mp[0] += 5; character.power++;				// hp+10, mp+5, power+1
		character.lv++; character.exp[1] = 0; character.exp[0] += character.lv * 10;		// lv+1, exp = 0, 필요 exp -> 레벨*10
	}

	// hp,mp 리젠
	if (character.tick[0] + 900 < tick) {									// 900ms 마다
		character.tick[0] = tick;											// hp&mp gen tick을 현재시간으로
		character.hp[1] += roundf(character.hp[0] * 0.01);			// 현재 hp를 최대hp의 0.01만큼 회복
		character.mp[1] += roundf(character.mp[0] * 0.05);		// 현재 mp를 최대mp의 0.05만큼 회복
	}
	// hp가 최대일 경우 -> 최대hp를 현재hp로
	if (character.hp[1] > character.hp[0])
		character.hp[1] = character.hp[0];
	// mp가 최대일 경우 -> 최대mp를 현재mp로
	if (character.mp[1] > character.mp[0])
		character.mp[1] = character.mp[0];

	// hp가 1보다 낮을 경우 -> 게임오버 ( tick = 0 )
	if (character.hp[1] < 1)
		tick = 0;

	// 피격 tick이 0보다 클경우 1 감소
	if (character.tick[5] > 0)
		character.tick[5] -= 1;

	// 'z' 키를 누르고 있으며, 체공을 하고 있지 않는 경우 -> 공격
	if (GetAsyncKeyState(0x5A) & 0x8000 && character.flyTime == 0) {
		attack = TRUE;
		character.motion[1] = TRUE;
	}

	// character.motion[1] = TRUE일 경우
	if (character.motion[1]) {
		// 공격 움직임. 150ms 마다 갱신 ( 모션 처리 시간 )
		if (tick > character.tick[2] + 150) {
			character.tick[2] = tick;		// 현재 시간을 attack tick으로 sync
			character.motion[2]++;		// 연속 attack 횟수
		}

		// 공격을 연속으로 3번 누른 후 다음 키가
		if (character.motion[2] > 3) {
			// 공격일 경우
			if (attack) {
				character.motion[2] = 1; character.motion[3]++;		// motion[2]: 4 -> 1, motion[3]: 0 -> 1
			}
			// 공격이 아닐 경우
			else {
				character.motion[1] = FALSE; character.motion[2] = 0; character.motion[3] = 1;
				// motion[1]: 1 -> 0, motion[2]: 4 -> 0, motion[3]: 1
			}

			if (character.motion[3] > 3)
				character.motion[3] = 1;			// motion[3]: 4 -> 1
		}
	}
	// character.motion[1] = FALSE 일 경우
	else {
		// move left
		if (GetAsyncKeyState(VK_LEFT) & 0x8000 && x > 1) {
			// 가속도가 -1 이상일때 -1로
			if (character.accel[0] > -1)
				character.accel[0] = -1;

			// direction = FALSE -> 왼쪽
			character.direction = FALSE;
			move = TRUE;
		}

		// move right
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000 && x < MAP_X_MAX - 2) {
			// 가속도가 1 미만일때 1로
			if (character.accel[0] < 1)
				character.accel[0] = 1;

			// direction = TRUE -> 오른쪽
			character.direction = TRUE;
			move = TRUE;
		}

		// 'x' 키를 누르고 있음 -> dash
		if (GetAsyncKeyState(0x58) & 0x8000 && character.tick[3] + 1200 <= tick && character.mp[1] >= 10) {
			// x가속도를 3(오른쪽) or -3(왼쪽) 으로 설정
			character.accel[0] = character.direction * 6 - 3;
			// dash tick을 현재 시간으로
			character.tick[3] = tick;
			// mp 10 소모
			character.mp[1] -= 10;
		}
	}

	// 캐릭터 중심 y좌표에서 + 3한 값이 floor 값과 같고 방향키 윗키를 누른 경우 -> jump
	if (GetAsyncKeyState(VK_UP) & 0x8000 && y + 3 == FLOOR_Y)
		character.accel[1] = -1.75;

	// 다리 움직임. 90ms 마다 갱신
	if (tick > character.tick[1] + 90) {
		character.tick[1] = tick;

		// motion[0]: 1 -> 2 -> 3 -> 4 -> 1 -> 2 -> 3 -> 4
		// 부드러운 움직임을 위해 3가지 모션을 사용
		if (move == TRUE)
			character.motion[0]++;
		// move가 FALSE일 경우 motion[0] = 0
		else
			character.motion[0] = 0;

		// 1~4 loop처리
		if (character.motion[0] > 3)
			character.motion[0] = 1;
	}

	// user가 움직인 값을 accel을 이용하여 coord 값 수정.
	// if, jump일 경우 flyTime과 accel[1]을 이용하여 coord 값을 수정.
	MoveControl(character.coord, character.accel, character.size, &character.flyTime);	

	// 피격 tick이 짝수일 때 -> 깜빡임을 구현하기 위해
	if (character.tick[5] % 2 == 0) {
		DrawSprite(x, y, character.size[0], character.size[1], sprite_character);	//draw character sprite

		// character의 방향에 따른 등 방향
		if (character.direction) {
			EditMap(x, y + 1, '(');
		}
		else {
			EditMap(x + 2, y + 1, ')');
		}

		// 오른쪽으로 가는 대쉬
		if (character.accel[0] > 1)
			DrawSprite(x - 2, y, 1, 3, "===");
		// 왼쪽으로 가는 대쉬
		if (character.accel[0] < -1)
			DrawSprite(x + 4, y, 1, 3, "===");

		// draw attack motion
		// 공격이 TRUE고, motion[2]가 0보다 클 경우
		if (character.motion[1] && character.motion[2] > 0) {
			// 3연속 공격일 경우
			if (character.motion[3] == 3) {
				// 3연속 공격 모션
				DrawSprite(x - 5 + 8 * character.direction, y, 5, 3, sprite_normalAttack[character.direction][character.motion[2] - 1]);
			}
			// 일반 공격
			else {
				// 공격 모션 1
				if (character.motion[2] == 2) {
					// draw hand
					EditMap(x - 2 + 6 * character.direction, y + 1, 'o');
					// draw weapon
					DrawSprite(x - 5 + 10 * character.direction, y + 1, 3, 1, sprite_weapon[character.direction][character.weapon]);
				}
				// 공격 모션 2
				else {
					// draw hand
					EditMap(x + 2 * character.direction, y + 1, 'o');
					// draw weapon
					DrawSprite(x - 3 + 6 * character.direction, y + 1, 3, 1, sprite_weapon[character.direction][character.weapon]);
				}
			}
		}
		// 공격이 FALSE 일 경우
		else {
			// 무기와 본체가 붙어있게 draw weapon sprite
			EditMap(x + character.direction * 2, y + 1, 'o');
			DrawSprite(x - 3 + 6 * character.direction, y + 1, 3, 1, sprite_weapon[character.direction][character.weapon]);
			
			// 움직임 motion이 3일 경우
			if (character.motion[0] == 3)
				EditMap(x + 1, y + 1, 'l');	// character의 몸통 부분 draw
		}

		// move가 TRUE 일 경우
		if (character.motion[0] > 0)
			DrawSprite(x, y + 2, 3, 1, sprite_character_leg[character.direction][character.motion[0] - 1]);	// draw leg motion
	}
}

// item이 drop될때 모션이나, 캐릭터와 충돌될 때 모션 구현 함수
void ControlItem(int index) {
	short x = objects[index]->coord[0], y = objects[index]->coord[1];
	short item_coord[2] = { x, y - 2 };
	short item_size[2] = { 5, 2 };

	if (objects[index]->tick[1] < tick) {
		objects[index]->tick[1] = tick * 2;
		// x 가속값을 랜덤 값으로 -> 왼쪽으로 튀어오름
		objects[index]->accel[0] = 1 - 2 * objects[index]->hp[0] / (float)RAND_MAX;
		// y 가속값을 랜덤 값으로 -> 위로 튀어오름
		objects[index]->accel[1] = -2 * objects[index]->hp[1] / (float)RAND_MAX;
	}

	// user와 item이 겹칠 경우
	if (CollisionCheck(item_coord, character.coord, item_size, character.size)) {
		// item 위에 E를 표시
		DrawSprite(x + 1, y - 5, 3, 1, "[E]");

		// user가 E를 누르면
		if (GetAsyncKeyState(0x45) & 0x8000) {
			// user의 weapon을 objects[index]->kind로 바꾸고
			character.weapon = objects[index]->kind;

			// objects[index] 를 Remove
			RemoveObject(index);
			return;
		}
	}

	// weapon의 sprite를 draw
	DrawSprite(x, y - 2, 5, 2, sprite_invenWeapon[objects[index]->kind]);

	// item drop 시 떨어지는 모션
	MoveControl(objects[index]->coord, objects[index]->accel, objects[index]->size, &objects[index]->flyTime);
}

// mob의 움직임 체력표시, 아이템 드롭 구현
void ControlEnemy(int index) {
	short x = objects[index]->coord[0], y = objects[index]->coord[1];
	// user의 weapon 좌표값
	short at_coord[2] = { character.coord[0] - 5 + 8 * character.direction, character.coord[1] }, at_size[2] = { 5, 3 };
	// rand값 (0 ~ 99)
	short item_code = rand() % 100;

	// mob의 hp가 1보다 작을 경우 -> mob을 처치했을 경우
	if (objects[index]->hp[1] < 1) {
		// mob 3마리 생성
		for (int i = 0; i < 3; i++)
			CreateObject(x + objects[index]->size[0] / 2, y + objects[index]->size[1] / 2, objects[index]->kind + 100);

		// 9% 확률로 1번째 무기 drop
		if (item_code >= 90)
			CreateObject(x + objects[index]->size[0] / 2 - 2, y, 1);

		// 4% 확률로 2번째 무기 drop
		if (item_code <= 3)
			CreateObject(x + objects[index]->size[0] / 2 - 2, y, 2);

		// 경험치 ++
		character.exp[1] += objects[index]->exp;

		// mob object 제거
		RemoveObject(index);
		return;
	}
	
	// hp 표시 시간 -> 피격 시 2초동안
	if (objects[index]->tick[0] + 2000 > tick)
		DrawNumber(x + objects[index]->size[0] / 2 - NumLen(objects[index]->hp[1]) / 2, y - 1, objects[index]->hp[1]);

	// attack이 TRUE 이고 무기가 mob과 충돌하였을 경우
	if (character.motion[2] == 1 && CollisionCheck(objects[index]->coord, at_coord, objects[index]->size, at_size)) {
		// 현재 시간을 hp 표시 시간 tick으로 설정
		objects[index]->tick[0] = tick;
		// mob의 현재 체력을 user의 power로 깎음
		objects[index]->hp[1] -= character.power;
		// y 가속값을 -0.55 -> 체공
		objects[index]->accel[1] = -0.55;

		// 3연속 공격일 경우 체력을 한번 더 깎음
		if (character.motion[3] == 3)
			objects[index]->hp[1] -= character.power;

		// user를 바라보고 있는 방향이 오른쪽일 경우, x 가속값을 -0.75로 설정 -> 왼쪽으로 튕겨나감
		if (EnemyPosition(x, objects[index]->size[0]))
			objects[index]->accel[0] = -0.75;
		// 왼쪽일 경우, x 가속값을 0.75로 설정 -> 오른쪽으로 튕겨나감
		else
			objects[index]->accel[0] = 0.75;
	}

	// mob kind가 100일 경우 -> mob id가 100일 경우
	if (objects[index]->kind == 100) {
		// y좌표가 바닥일 경우 motion[0]을 0으로
		if (y + objects[index]->size[1] == FLOOR_Y)
			objects[index]->motion[0] = 0;
		// 아니면 1로
		else
			objects[index]->motion[0] = 1;

		// mob 움직임 구현
		if (objects[index]->tick[1] + objects[index]->tick[2] < tick) {
			objects[index]->tick[1] = tick;
			objects[index]->tick[2] = 1000 + rand() % 1000;
			// y 가속값이 random하게 지정됨 ( 이때, 비율은 x 가속 값의 절반이 되어야 포물선으로 떨어짐 )
			objects[index]->accel[1] = rand() / (float)RAND_MAX / 2 - 1.2;

			// user를 바라보고 있는 mob의 방향이 오른쪽일 경우, x 가속 값이 양수 random값으로 지정됨 -> 오른쪽으로 랜덤 크기로 점프
			if (EnemyPosition(x, objects[index]->size[0]))
				objects[index]->accel[0] = 2.4 - rand() / (float)RAND_MAX;
			// user를 바라보고 있는 mob의 방향이 왼쪽일 경우, x 가속 값이 음수 random값으로 지정됨 -> 왼쪽으로 랜덤 크기로 점프
			else
				objects[index]->accel[0] = rand() / (float)RAND_MAX - 2.4;
		}

		// user의 피격 tick이 0일 경우 && user와 mob이 충돌
		if (character.tick[5] == 0 && CollisionCheck(objects[index]->coord, character.coord, objects[index]->size, character.size)) {
			// user의 피격 tick을 100으로 설정, user의 체력 10 감소
			character.tick[5] = 100;
			character.hp[1] -= 10;
		}

		// mob의 sprite를 draw
		DrawSprite(x, y, objects[index]->size[0], objects[index]->size[1], sprite_enemy1[objects[index]->motion[0]]);
	}

	// mob의 movement를 설정
	MoveControl(objects[index]->coord, objects[index]->accel, objects[index]->size, &objects[index]->flyTime);
}

// Score를 올려주는 Particle에 대한 함수
void ControlParticle(int index) {
	short x = objects[index]->coord[0], y = objects[index]->coord[1];
	short money_size[2] = { 2, 2 };
	short money_coord[2] = { x, y - 1 };

	// object의 kind가 200일 경우 -> particle id가 200일 경우
	if (objects[index]->kind == 200) {
		if (objects[index]->tick[1] < tick) {
			objects[index]->tick[1] = tick * 2;
			objects[index]->accel[0] = 2 - 4 * objects[index]->hp[0] / (float)RAND_MAX;
			objects[index]->accel[1] = -2 * objects[index]->hp[1] / (float)RAND_MAX;
		}

		// user와 particle의 충돌이 발생할 경우
		if (CollisionCheck(money_coord, character.coord, money_size, character.size)) {
			// score + 100
			character.score += 100;

			// particle 제거
			RemoveObject(index); 
			return;
		}

		// x, y 좌표에 particle draw
		EditMap(x, y - 1, '@');
	}

	// particle의 가속값과 좌표값을 통하여 이동 값 계산
	MoveControl(objects[index]->coord, objects[index]->accel, objects[index]->size, &objects[index]->flyTime);
}

// object 각각의 값을 tick에 맞게 mapData 수정
void ControlObject() {
	for (int i = 0; i < OBJECT_MAX; i++) {
		// object가 있을 경우
		if (objects[i]) {
			// objects[i]가 item일 경우
			if (objects[i]->kind < 100)
				ControlItem(i);
			// objects[i]가 mob일 경우
			else if (objects[i]->kind > 99 && objects[i]->kind < 200)
				ControlEnemy(i);
			// objects[i]가 particle일 경우
			else
				ControlParticle(i);
		}
	}
}

// kind로 분류하여 Object생성
void CreateObject(short x, short y, short kind) {
	int index = 0;
	Object* obj = 0;

	// objects 중 빈 공간 탐색
	while (TRUE) {
		if (!objects[index])
			break;

		// 빈 공간이 없을 경우 return
		if (index == OBJECT_MAX)
			return;

		index++;
	}

	// Object Heap 할당
	obj = (Object*)malloc(sizeof(Object));
	// Objects에 만든 Object 삽입
	objects[index] = obj;
	memset(obj, 0, sizeof(Object));

	// object의 초기 값을 설정
	obj->kind = kind;
	obj->coord[0] = x; obj->coord[1] = y;
	obj->tick[0] = 0;

	// item이나 particle일 경우
	if (kind < 100 || kind > 199) {
		// hp 값을 rand num으로
		obj->hp[0] = rand();
		obj->hp[1] = rand();
		obj->tick[1] = 0;
		obj->tick[2] = 0;
		obj->tick[3] = 0;
	}

	// mob일 경우
	if (kind > 99 && kind < 200) {
		// 초기값 설정 kind - 100 => 기존에 설정해두었던 mob의 stat을 불러옴
		obj->hp[0] = stat_enemy[kind - 100][0];
		obj->hp[1] = obj->hp[0];

		obj->exp = stat_enemy[kind - 100][1];

		obj->size[0] = stat_enemy[kind - 100][2];
		obj->size[1] = stat_enemy[kind - 100][3];

		obj->tick[1] = stat_enemy[kind - 100][4];
		obj->tick[2] = stat_enemy[kind - 100][5];
		obj->tick[3] = stat_enemy[kind - 100][6];
	}
}

// Object를 지우는 함수
void RemoveObject(int index) {
	// 해당 index의 object를 free
	free(objects[index]);
	// index의 objects를 비움
	objects[index] = 0;
}

// 1과 2의 충돌 검사 함수
bool CollisionCheck(short coord1[], short coord2[], short size1[], short size2[]) {
	// 1의 x값이 2의 x값에서 1의 x size를 뺀 값 보다 크면서
	// 1의 x값이 2의 x값에서 2의 x size를 더한 값 보다 작고
	// 1의 y값이 2의 y값에서 1의 y size를 뺀 값 보다 크면서
	// 1의 y값이 2의 y값에서 2의 y size를 더한 값 보다 작을 경우  -> 모두 충족시켜야 함. (x, y가 모두 겹쳐질 경우를 뜻함.)
	if (coord1[0] > coord2[0] - size1[0] && coord1[0] < coord2[0] + size2[0]
		&& coord1[1] > coord2[1] - size1[1] && coord1[1] < coord2[1] + size2[1])
		return TRUE;
	else
		return FALSE;
}

// mob이 user를 향해 바라보고 있는 방향
bool EnemyPosition(short x, short size_x) {
	// user의 x 좌표가 (mob의 x좌표 + x의 크기 / 2) 보다 작을 경우 -> mob은 왼쪽을 바라봄.
	if (character.coord[0] + 1 < x + floor(size_x / 2 + 0.5))
		return FALSE;
	// 아니면 mob은 오른쪽을 바라봄.
	else
		return TRUE;
}

// x, y좌표값과 x, y가속값을 이용하여 움직임을 구현하는 함수
void MoveControl(short coord[], float accel[], short size[], float* flyTime) {
	// 최종 x, y가속값 -> x, y 좌표 이동값
	float x_value = accel[0], y_value = accel[1];

	// y좌표와 y크기를 합한 값이 바닥의 좌표와 같을 경우 -> 바닥에 있음. -> 체공시간 == 0
	if (coord[1] + size[1] == FLOOR_Y) {
		*flyTime = 0;
	}
	// 다를경우 -> 공중에 떠 있음.
	else {
		// flyTime을 증가시키고 y가속값에서 flyTime 값을 더함
		// 이때, 공중에 뜸을 표현하기 위하여 y가속값에 음수 값을 넣는데 이 시간을 0.05로 나누면
		// 체공 시간을 구할 수 있음.
		*flyTime += 0.05;
		accel[1] += *flyTime;
	}

	// x가속값과 y가속값이 0이 아닐경우 -> 움직이고 있을 경우
	if (x_value != 0 || y_value != 0) {
		// x좌표값 + x 가속값이 1보다 작을 경우 -> x 최소 값을 벗어난 경우 -> x가속값을 x 최소값을 넘지 않게(x_value > 1)
		if (coord[0] + x_value < 1)
			x_value = 1 - coord[0];
		// x좌표값 + x크기값 + x가속값이 X 최대값보다 클 경우 -> x가속값을 x 최대값을 넘지 않게(x_value < MAP_X_MAX)
		if (coord[0] + size[0] + x_value > MAP_X_MAX)
			x_value = MAP_X_MAX - size[0] - coord[0];
		// y좌표값 + y크기값 + y가속값이 바닥 보다 클 경우 -> y가속값을 수정
		if (coord[1] + size[1] + y_value > FLOOR_Y)
			y_value = FLOOR_Y - coord[1] - size[1];
	}

	// x좌표값에 가속값을 더해서 저장. y좌표값에 가속값을 더해서 저장. -> user나 object의 x, y값이 바뀜 -> 이동
	// 만약, x_value가 양수일 경우 오른쪽으로, 음수일 경우 왼쪽으로 이동.
	coord[0] += floor(x_value + 0.5); coord[1] += floor(y_value + 0.5);

	// x가속값이 0보다 클 경우 -0.2, 작을 경우 +0.2
	if (accel[0] > 0) accel[0] -= 0.2; if (accel[0] < 0) accel[0] += 0.2;
	// y가속값이 0보다 클 경우 -0.1, 작을 경우 +0.1
	if (accel[1] > 0) accel[1] -= 0.1; if (accel[1] < 0) accel[1] += 0.1;
}

// box를 그리는 함수
void DrawBox(short x, short y, short size_x, short size_y) {
	// 최상단 좌, 우 꼭짓점
	EditMap(x, y, '.'); EditMap(x + size_x - 1, y, '.');
	// 최하단 좌, 우 꼭짓점
	EditMap(x, y + size_y - 1, '\''); EditMap(x + size_x - 1, y + size_y - 1, '\'');

	// 가로줄 그리기
	for (int i = 1; i < size_x - 1; i++) {
		EditMap(x + i, y, '-'); EditMap(x + i, y + size_y - 1, '-');
	}
	// 세로줄 그리기
	for (int i = 1; i < size_y - 1; i++) {
		EditMap(x, y + i, '|'); EditMap(x + size_x - 1, y + i, '|');
	}
}

// 숫자를 draw 하는 함수
void DrawNumber(short x, short y, int num) {
	// tmp 숫자 저장 임시 변수, len 숫자 길이가 담기는 변수, cnt 반복문 iterator
	int tmp = num, len = NumLen(tmp), cnt = len;
	// 숫자 -> 문자 결과값 저장 변수
	char* str = malloc(sizeof(char) * len);
	memset(str, 0, sizeof(char) * len);

	do {
		// 숫자 길이 1씩 감소시키면서
		cnt--;
		// ascii code로 1의 자리부터 차례대로 넣음.
		str[cnt] = (char)(tmp % 10 + 48);
		tmp /= 10;
	} while (tmp != 0);

	// x, y 좌표에 숫자를 draw
	DrawSprite(x, y, len, 1, str);
	free(str);
}

// sprite를 draw하는 함수
void DrawSprite(short x, short y, short size_x, short size_y, const char spr[]) {
	for (int i = 0; i < size_y; i++) {
		for (int n = 0; n < size_x; n++)
			// x와 y를 기준으로 n, i를 증가시키면서 받아온 spr에 담긴 값으로 mapData를 edit
			EditMap(x + n, y + i, spr[i * size_x + n]);
	}
}

// 가져온 str배열에 max_value만큼 str_s로 가득 채움
void FillMap(char str[], char str_s, int max_value) {
	for (int i = 0; i < max_value; i++)
		str[i] = str_s;
}

// mapData배열에서 x, y 가 유효한 좌표일 경우, str로 mapData를 edit
void EditMap(short x, short y, char str) {
	if (x > 0 && y > 0 && x - 1 < MAP_X_MAX && y - 1 < MAP_Y_MAX)
		mapData[(y - 1) * MAP_X_MAX + x - 1] = str;
}

// 숫자의 길이를 구하는 함수
int NumLen(int num) {
	int tmp = num, len = 0;

	// 0이면 길이가 1이므로 1을 return
	if (num == 0) {
		return 1;
	}
	else {
		// tmp를 10으로 나누면서 len을 증가시킴 -> 자릿수마다 len 증가
		while (tmp != 0) {
			tmp /= 10;
			len++;
		}
	}

	return len;
}