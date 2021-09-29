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
#define CHOP(x) x[strlen(x) - 1] = ' '		// insert�Ҷ� ���ڿ� chopping
#define FALSE 0					// ����
#define TRUE 1					// ��
#define MAP_X_MAX 96			// �ִ� X ��ǥ
#define MAP_Y_MAX 32			// �ִ� Y ��ǥ
#define FLOOR_Y 26				// �ٴ� Y ��ǥ
#define OBJECT_MAX 32			// ������ �� �ִ� Object�� �ִ�
#define SPAWN_TIME 15000		// mobs�� ���� �ֱ� (15000ms)

// user ����ü ����
typedef struct _Character {
	short coord[2], size[2];	// user�� x, y ��ǥ�� �� ũ��
	float accel[2], flyTime;	// user�� ���Ӱ�(0: x, 1: y) �� ü�� �ð�
	bool direction;				// user�� ����, true�� ������, false�� ����

	// user status
	char name[16];							// user �̸�. �Ʒ� score�� �Բ� �����.
	int lv, exp[2], score, hp[2], mp[2];	// exp[] -> 0=exp required, 1=current exp // hp[], mp[] -> 0=max value, 1=current value
	short power, weapon;					// user�� ���ݷ�, ����

	// animation control
	// 0=leg_motion, 1,2,3=attack_motion(1, 2, 3), invincibility motion
	short motion[4];

	// 0=hp, mp ȸ�� tick, 1=�̵� ������ tick, 2=���� ������ tick, 3=�뽬 ������ tick, 4=�ǰ� ������ tick
	unsigned int tick[6];
} Character;

// object(items, mobs, particles) ����ü ����
typedef struct _Object {		// enemies, projectiles, particles, etc.
	short coord[2], size[2];	// object�� x, y ��ǥ�� �� ũ��
	float accel[2], flyTime;		// object�� ���Ӱ� �� ü�� �ð�
	bool direction;				// object�� ����, true�� ������, false�� ����

	short kind;		// 1~99: items, 100~199: mobs, 200~: particles
	int hp[2], exp;	// hp: this value is used randomly for item or particle object
	short dam;

	short motion[3];		// motion
	unsigned int tick[4];	// 0: hpshow time(enemy) or active time(projecticles, particles)
} Object;

// user ����ü �ʱ�ȭ
Character character = {
	{MAP_X_MAX / 2, MAP_Y_MAX / 2}, {3, 3},		// x,y �� ���߾�, ũ�� 3 by 3
	{0, 0},	0,
	1,												// direction
	"",												// name
	1, {100, 0}, 0, {100, 100}, {50, 50},			// lv.1, hp 100, mp 50
	10, 0,											// power: 10
	{0, 1, 0, 0},									// motion
	{0, 0, 0, 0, 0}									// tick
};
Object** objects;

unsigned int tick = 0;											// ���� ��� �ð�
unsigned int spon_tick = 0;										// ���� ���� �ֱ�
char sprite_floor[MAP_X_MAX];									// �ٴ� �迭
char mapData[MAP_X_MAX * (MAP_Y_MAX + 1)];						// array for graphics

const short stat_enemy[2][7] ={			// mobs�� hp, exp, size(x y), tick (2 3 4)
	{150, 30, 4, 3, 0, 1000, 0},
	{300, 50, 5, 5, 0, 500, 0}
};
const short stat_weapon[3] = { 5, 10, 15 };
const char sprite_character[10] = " 0  | _^_";			// user character 3 * 3
const char sprite_character_leg[2][3][4] = {
	{"-^.", "_^\'", "_^."},								// ���� move�� user leg ������ ( 3 motions )
	{".^-", "\'^_", ".^_"}								// ������ move�� user leg ������ ( 3 motions )
};
const char sprite_normalAttack[2][3][16] = {
	{" .- o          ", " .   (   o \'   ", "         o \'-  "},		// ���� 3��Ÿ �� user attack ������ ( 3 motions )
	{"o -.           ", "   . o   )   \' ", "     o      -\' "}			// ������ 3��Ÿ �� user attack ������ ( 3 motions )
};
const char sprite_weapon[2][3][4] = {
	{"---", "--+", "<=+"},											// ���� ���� ���� sprite
	{"---", "+--", "+=>"}											// ������ ���� ���� sprite
};
const char sprite_invenWeapon[3][11] = { "   /   /  ", "   /  '*. ", "  |   \"+\" " };			// weaponâ�� �� ���� sprite
const char sprite_enemy1[2][13] = { " __ (  )----", " __ [  ]\'--\'" };							// mob sprite

/*
	�Լ� ����
*/
void StartGame();					// ���� ����
void UpdateGame();					// ���� ���� ������Ʈ
void ExitGame();					// ���� ����
void SetConsole();					// �ܼ� â ����
void ControlUI();					// UI ���� ������Ʈ
void ControlCharacter();			// user ���� ������Ʈ
void ControlObject();				// mobs, items, particles ���� ������Ʈ
void ControlItem(int index);		// item ���� ������Ʈ
void ControlEnemy(int index);		// mob ���� ������Ʈ
void ControlParticle(int index);	// particle ���� ������Ʈ
void CreateObject(short x, short y, short kind);			// mobs, items, particles ����
void RemoveObject(int index);								// mobs, items, particles ����
bool EnemyPosition(short x, short size_x);					// user�� �ٶ󺸰��ִ� mobs�� ����
bool CollisionCheck(short coord1[], short coord2[], short size1[], short size2[]);		// user <-> objects �浹 �˻�
void MoveControl(short coord[], float accel[], short size[], float* flyTime);			// motion Control
void DrawBox(short x, short y, short size_x, short size_y);								// size_x, size_y ũ���� ���ڸ� x, y ��ǥ�� draw
void DrawNumber(short x, short y, int num);												// x, y ��ǥ�� number�� draw ( ���� ���� )
void DrawSprite(short x, short y, short size_x, short size_y, const char spr[]);		// size_x, size_y ũ���� sprite�� x, y ��ǥ�� draw
void FillMap(char str[], char str_s, int max_value);											// �迭 �ʱ�ȭ
void EditMap(short x, short y, char str);														// mapData���� x, y ��ǥ�� ���� ����
int NumLen(int num);																				// ���� ���� return
void gotoxy(int x, int y);																			// �ܼ� Ŀ�� ��ġ �̵�
int menu();				// ���� �޴�
void ViewScore();		// DB�� �����Ͽ� ���ھ� ǥ��
void InputScore();		// DB�� �����Ͽ� ���ھ� DB�� �Է�

int main() {
	StartGame();			// main �Լ� ���� �� ���� ����, �ʱ�ȭ ������ ����

	while (TRUE) {
		if (tick + 30 < GetTickCount64()) {				// �ð��� 30ms �귶����
			tick = GetTickCount64();					// ���� �ð��� tick����

			UpdateGame();								// ���� ���� update

			if (tick == 0)								// update ���� tick�� 0�� �Ǹ� break
				break;
		}
	}

	ExitGame();											// tick�� 0�� ���, ���� ����
	return 0;
}

void gotoxy(int x, int y) {
	COORD Pos = { x, y };		// �� Pos ���� x, y��
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), Pos);		// �ܼ��� Ŀ���� Pos�� �̵�
}

// �޴� ��� �Լ�
int menu() {
	// ��ǥ�� �̵��ϸ� �޴��� ���
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

	// �޴����� �Է¹��� ���� return
	int re;
	scanf("%d", &re);
	rewind(stdin);

	return re;
	
	system("cls");
}

// Score ǥ�� �Լ�
void ViewScore() {
	MYSQL* connection = NULL, conn;		// connection: mysql connection, conn: mysql init
	MYSQL_RES* sql_result;				// query�� ���� result
	MYSQL_ROW sql_row;					// result�� row�� fetch�� �迭

	int query_stat = 0;					// query�� ��� (���� or ����)

	mysql_init(&conn);		// mysql initialization

	mysql_options(&conn, MYSQL_SET_CHARSET_NAME, "euckr");			// mysql charset_name�� euckr��
	mysql_options(&conn, MYSQL_INIT_COMMAND, "SET NAMES euckr");	// mysql init_command�� euckr��

	// mysql connection �κ�
	connection = mysql_real_connect(&conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 3306, (char*)NULL, 0);
	
	// connection ����
	if (connection == NULL) {
		fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
		return 1;
	}

	// select ���� ����
	query_stat = mysql_query(connection, "select * from scoretable order by score desc limit 10");
	
	// query stat�� 0�� �ƴ� ��� -> query ���� �������� ���� �߻�
	if (query_stat != 0) {
		fprintf(stderr, "Mysql query error : %s", mysql_error(&conn));
		return 1;
	}

	// query result ����
	sql_result = mysql_store_result(connection);
	system("cls");
	gotoxy(38, 8);
	printf("Top 10 Users!\n");
	gotoxy(25, 10);
	printf("��������������������������������������������������������������������������������\n");
	int i = 11;
	// sql_result���� row�� �̾Ƽ� sql_row�� �����ϴµ� �� ���� NULL�� �ƴҶ� ���� -> ��� result ���� ���� row�� ���
	while ((sql_row = mysql_fetch_row(sql_result)) != NULL) {
		gotoxy(25, i);
		printf("��\t%s\t\t��\t%s\t��\n", sql_row[0], sql_row[1]);		// sql_row[0]: name, sql_row[1]: score
		i++;
	}
	gotoxy(25, i);
	printf("��������������������������������������������������������������������������������\n");

	// sql_result free
	mysql_free_result(sql_result);

	// mysql connection close
	mysql_close(connection);
}

// score ���� �Լ�
void InputScore() {
	MYSQL* connection = NULL, conn;		// connection: mysql connection, conn: mysql init

	int query_stat = 0;					// query�� ��� (���� or ����)
	char query[255];					// ������ query

	mysql_init(&conn);		// mysql initialization

	mysql_options(&conn, MYSQL_SET_CHARSET_NAME, "euckr");			// mysql charset_name�� euckr��
	mysql_options(&conn, MYSQL_INIT_COMMAND, "SET NAMES euckr");	// mysql init_command�� euckr��

	// mysql connection �κ�
	connection = mysql_real_connect(&conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 3306, (char*)NULL, 0);

	// connection ����
	if (connection == NULL) {
		fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
		return 1;
	}

	// name chopping�� ���� heap �Ҵ�
	char* name = malloc(sizeof(char) * strlen(character.name));
	CHOP(name);

	// query�� ������ query ������ insert�� ����
	sprintf(query, "insert into scoretable values " "('%s', '%d')", character.name, character.score);
	// mysql�� connection�� query�� ����
	query_stat = mysql_query(connection, query);
	// ���޵� query�� ���� �߻� ��
	if (query_stat != 0) {
		fprintf(stderr, "Mysql query error : %s", mysql_error(&conn));
		return 1;
	}

	// �Ҵ���� ���� free
	free(name);
	// mysql connection close
	mysql_close(connection);
}

void StartGame() {
	SetConsole();								// console ���� ( Ŀ�� �Ⱥ��̰�, title )
	srand((unsigned int)time(NULL));			// rand �Լ��� ���� srand

	int ch = 0;

	while (1) {
		ch = menu();
		// play game
		if (ch == 1) {
			break;
		}
		// view score
		else if (ch == 2) {
			ViewScore();		// db�� ����� ������ ǥ��
			_getch();
			system("cls");
		}
		// how to play
		else if (ch == 3) {
			system("cls");
			gotoxy(35, 10);
			printf("��, ��:\t�̵�");
			gotoxy(35, 12);
			printf("��:\t����");
			gotoxy(35, 14);
			printf("z:\t����");
			gotoxy(35, 16);
			printf("x:\t�뽬");
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
	scanf("%[^\n]s", character.name);		// �̸��� character ����ü�� ����
	rewind(stdin);

	gotoxy(0, 0);									// console Ŀ���� 0, 0���� �̵�

	FillMap(sprite_floor, '=', MAP_X_MAX);	// �ٴ� �迭 �ʱ�ȭ

	objects = (Object**)malloc(sizeof(Object*) * OBJECT_MAX);		// objects �� Objectũ���� 32��ŭ �Ҵ� -> 32���� object ���� ����
	memset(objects, 0, sizeof(Object*) * OBJECT_MAX);				// �ʱ�ȭ
}

void UpdateGame() {
	FillMap(mapData, ' ', MAP_X_MAX * MAP_Y_MAX);	// mapData�� ������ �������� �ʱ�ȭ

	ControlCharacter();	// mapData(character) �� ������Ʈ
	ControlObject();		// mapData(enemy, projecticles, particles, etc...) �� ������Ʈ
	ControlUI();				// mapData(UI) �� ������Ʈ

	if (spon_tick + SPAWN_TIME < tick) {		// ���� �ֱⰡ ������ 
		spon_tick = tick;								// ���� �ð��� ���� �ֱ�� �缳��
		CreateObject(rand() % 90, 10, 100);		// mob ��ȯ
		CreateObject(rand() % 90, 10, 100);		// mob ��ȯ
		CreateObject(rand() % 90, 10, 100);		// mob ��ȯ
	}

	printf("%s", mapData);	// console�� mapData Print
}

void ExitGame() {
	for (int i = 0; i < OBJECT_MAX; i++) {
		if (objects[i])
			free(objects[i]);			// free�� ���� ���� object ����ü�� free
	}

	free(objects);					// objects ����ü�� free

	system("cls");
	gotoxy(35, 8);
	printf("Game Over!");
	InputScore();					// db�� ������ ����

	gotoxy(35, 10);
	printf("Save Success!\n");
	_getch();
}

void SetConsole() {
	system("mode con:cols=96 lines=32");			// �� 96, �� 32 ũ��
	system("title Monster Hunter");								// title ����

	HANDLE hConsole;
	CONSOLE_CURSOR_INFO ConsoleCursor;

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	ConsoleCursor.bVisible = 0;							// Ŀ�� �Ⱥ��̰�
	ConsoleCursor.dwSize = 1;							// Ŀ�� �β� 1

	SetConsoleCursorInfo(hConsole, &ConsoleCursor);		// console cursor info�� setting
}

// mapData�� UI ������ input�ϴ� �Լ�
void ControlUI() {
	int expPer = roundf(character.exp[1] * 100 / character.exp[0]);			// exp ���� %
	int len;			// ���� sprite�� ����

	DrawSprite(1, FLOOR_Y, MAP_X_MAX, 1, sprite_floor);	// �ٴ� draw

	DrawBox(1, 1, 35, 8);				// UI box draw 35 * 8

	// inven�� ���� draw
	//  ex)
	// 	Weapon
	//  .-----.
	//  |    / |
	//  |  /   |
	//  '-----'
	//
	DrawBox(27, 4, 7, 4);				// box draw 7 * 4
	DrawSprite(28, 5, 5, 2, sprite_invenWeapon[character.weapon]);		// weapon sprite draw
	DrawSprite(28, 3, 6, 1, "Weapon");			// "Weapon"���� draw

	// �̸�, ����, hp, mp, ����ġ draw
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
	if (!expPer) {									// exp�� 0�ϰ��
		EditMap(len, 2, '0');	len++;		// 0
	}
	else {											// �ƴϸ�
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

// mapData�� user�� ������, ���ݸ���� input�ϴ� �Լ�
void ControlCharacter() {
	// �̵�, ����
	bool move = FALSE, attack = FALSE;
	// ĳ���� x, y ��ǥ
	int x = character.coord[0], y = character.coord[1];

	// ������
	if (character.exp[1] >= character.exp[0]) {
		character.hp[0] += 10; character.mp[0] += 5; character.power++;				// hp+10, mp+5, power+1
		character.lv++; character.exp[1] = 0; character.exp[0] += character.lv * 10;		// lv+1, exp = 0, �ʿ� exp -> ����*10
	}

	// hp,mp ����
	if (character.tick[0] + 900 < tick) {									// 900ms ����
		character.tick[0] = tick;											// hp&mp gen tick�� ����ð�����
		character.hp[1] += roundf(character.hp[0] * 0.01);			// ���� hp�� �ִ�hp�� 0.01��ŭ ȸ��
		character.mp[1] += roundf(character.mp[0] * 0.05);		// ���� mp�� �ִ�mp�� 0.05��ŭ ȸ��
	}
	// hp�� �ִ��� ��� -> �ִ�hp�� ����hp��
	if (character.hp[1] > character.hp[0])
		character.hp[1] = character.hp[0];
	// mp�� �ִ��� ��� -> �ִ�mp�� ����mp��
	if (character.mp[1] > character.mp[0])
		character.mp[1] = character.mp[0];

	// hp�� 1���� ���� ��� -> ���ӿ��� ( tick = 0 )
	if (character.hp[1] < 1)
		tick = 0;

	// �ǰ� tick�� 0���� Ŭ��� 1 ����
	if (character.tick[5] > 0)
		character.tick[5] -= 1;

	// 'z' Ű�� ������ ������, ü���� �ϰ� ���� �ʴ� ��� -> ����
	if (GetAsyncKeyState(0x5A) & 0x8000 && character.flyTime == 0) {
		attack = TRUE;
		character.motion[1] = TRUE;
	}

	// character.motion[1] = TRUE�� ���
	if (character.motion[1]) {
		// ���� ������. 150ms ���� ���� ( ��� ó�� �ð� )
		if (tick > character.tick[2] + 150) {
			character.tick[2] = tick;		// ���� �ð��� attack tick���� sync
			character.motion[2]++;		// ���� attack Ƚ��
		}

		// ������ �������� 3�� ���� �� ���� Ű��
		if (character.motion[2] > 3) {
			// ������ ���
			if (attack) {
				character.motion[2] = 1; character.motion[3]++;		// motion[2]: 4 -> 1, motion[3]: 0 -> 1
			}
			// ������ �ƴ� ���
			else {
				character.motion[1] = FALSE; character.motion[2] = 0; character.motion[3] = 1;
				// motion[1]: 1 -> 0, motion[2]: 4 -> 0, motion[3]: 1
			}

			if (character.motion[3] > 3)
				character.motion[3] = 1;			// motion[3]: 4 -> 1
		}
	}
	// character.motion[1] = FALSE �� ���
	else {
		// move left
		if (GetAsyncKeyState(VK_LEFT) & 0x8000 && x > 1) {
			// ���ӵ��� -1 �̻��϶� -1��
			if (character.accel[0] > -1)
				character.accel[0] = -1;

			// direction = FALSE -> ����
			character.direction = FALSE;
			move = TRUE;
		}

		// move right
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000 && x < MAP_X_MAX - 2) {
			// ���ӵ��� 1 �̸��϶� 1��
			if (character.accel[0] < 1)
				character.accel[0] = 1;

			// direction = TRUE -> ������
			character.direction = TRUE;
			move = TRUE;
		}

		// 'x' Ű�� ������ ���� -> dash
		if (GetAsyncKeyState(0x58) & 0x8000 && character.tick[3] + 1200 <= tick && character.mp[1] >= 10) {
			// x���ӵ��� 3(������) or -3(����) ���� ����
			character.accel[0] = character.direction * 6 - 3;
			// dash tick�� ���� �ð�����
			character.tick[3] = tick;
			// mp 10 �Ҹ�
			character.mp[1] -= 10;
		}
	}

	// ĳ���� �߽� y��ǥ���� + 3�� ���� floor ���� ���� ����Ű ��Ű�� ���� ��� -> jump
	if (GetAsyncKeyState(VK_UP) & 0x8000 && y + 3 == FLOOR_Y)
		character.accel[1] = -1.75;

	// �ٸ� ������. 90ms ���� ����
	if (tick > character.tick[1] + 90) {
		character.tick[1] = tick;

		// motion[0]: 1 -> 2 -> 3 -> 4 -> 1 -> 2 -> 3 -> 4
		// �ε巯�� �������� ���� 3���� ����� ���
		if (move == TRUE)
			character.motion[0]++;
		// move�� FALSE�� ��� motion[0] = 0
		else
			character.motion[0] = 0;

		// 1~4 loopó��
		if (character.motion[0] > 3)
			character.motion[0] = 1;
	}

	// user�� ������ ���� accel�� �̿��Ͽ� coord �� ����.
	// if, jump�� ��� flyTime�� accel[1]�� �̿��Ͽ� coord ���� ����.
	MoveControl(character.coord, character.accel, character.size, &character.flyTime);	

	// �ǰ� tick�� ¦���� �� -> �������� �����ϱ� ����
	if (character.tick[5] % 2 == 0) {
		DrawSprite(x, y, character.size[0], character.size[1], sprite_character);	//draw character sprite

		// character�� ���⿡ ���� �� ����
		if (character.direction) {
			EditMap(x, y + 1, '(');
		}
		else {
			EditMap(x + 2, y + 1, ')');
		}

		// ���������� ���� �뽬
		if (character.accel[0] > 1)
			DrawSprite(x - 2, y, 1, 3, "===");
		// �������� ���� �뽬
		if (character.accel[0] < -1)
			DrawSprite(x + 4, y, 1, 3, "===");

		// draw attack motion
		// ������ TRUE��, motion[2]�� 0���� Ŭ ���
		if (character.motion[1] && character.motion[2] > 0) {
			// 3���� ������ ���
			if (character.motion[3] == 3) {
				// 3���� ���� ���
				DrawSprite(x - 5 + 8 * character.direction, y, 5, 3, sprite_normalAttack[character.direction][character.motion[2] - 1]);
			}
			// �Ϲ� ����
			else {
				// ���� ��� 1
				if (character.motion[2] == 2) {
					// draw hand
					EditMap(x - 2 + 6 * character.direction, y + 1, 'o');
					// draw weapon
					DrawSprite(x - 5 + 10 * character.direction, y + 1, 3, 1, sprite_weapon[character.direction][character.weapon]);
				}
				// ���� ��� 2
				else {
					// draw hand
					EditMap(x + 2 * character.direction, y + 1, 'o');
					// draw weapon
					DrawSprite(x - 3 + 6 * character.direction, y + 1, 3, 1, sprite_weapon[character.direction][character.weapon]);
				}
			}
		}
		// ������ FALSE �� ���
		else {
			// ����� ��ü�� �پ��ְ� draw weapon sprite
			EditMap(x + character.direction * 2, y + 1, 'o');
			DrawSprite(x - 3 + 6 * character.direction, y + 1, 3, 1, sprite_weapon[character.direction][character.weapon]);
			
			// ������ motion�� 3�� ���
			if (character.motion[0] == 3)
				EditMap(x + 1, y + 1, 'l');	// character�� ���� �κ� draw
		}

		// move�� TRUE �� ���
		if (character.motion[0] > 0)
			DrawSprite(x, y + 2, 3, 1, sprite_character_leg[character.direction][character.motion[0] - 1]);	// draw leg motion
	}
}

// item�� drop�ɶ� ����̳�, ĳ���Ϳ� �浹�� �� ��� ���� �Լ�
void ControlItem(int index) {
	short x = objects[index]->coord[0], y = objects[index]->coord[1];
	short item_coord[2] = { x, y - 2 };
	short item_size[2] = { 5, 2 };

	if (objects[index]->tick[1] < tick) {
		objects[index]->tick[1] = tick * 2;
		// x ���Ӱ��� ���� ������ -> �������� Ƣ�����
		objects[index]->accel[0] = 1 - 2 * objects[index]->hp[0] / (float)RAND_MAX;
		// y ���Ӱ��� ���� ������ -> ���� Ƣ�����
		objects[index]->accel[1] = -2 * objects[index]->hp[1] / (float)RAND_MAX;
	}

	// user�� item�� ��ĥ ���
	if (CollisionCheck(item_coord, character.coord, item_size, character.size)) {
		// item ���� E�� ǥ��
		DrawSprite(x + 1, y - 5, 3, 1, "[E]");

		// user�� E�� ������
		if (GetAsyncKeyState(0x45) & 0x8000) {
			// user�� weapon�� objects[index]->kind�� �ٲٰ�
			character.weapon = objects[index]->kind;

			// objects[index] �� Remove
			RemoveObject(index);
			return;
		}
	}

	// weapon�� sprite�� draw
	DrawSprite(x, y - 2, 5, 2, sprite_invenWeapon[objects[index]->kind]);

	// item drop �� �������� ���
	MoveControl(objects[index]->coord, objects[index]->accel, objects[index]->size, &objects[index]->flyTime);
}

// mob�� ������ ü��ǥ��, ������ ��� ����
void ControlEnemy(int index) {
	short x = objects[index]->coord[0], y = objects[index]->coord[1];
	// user�� weapon ��ǥ��
	short at_coord[2] = { character.coord[0] - 5 + 8 * character.direction, character.coord[1] }, at_size[2] = { 5, 3 };
	// rand�� (0 ~ 99)
	short item_code = rand() % 100;

	// mob�� hp�� 1���� ���� ��� -> mob�� óġ���� ���
	if (objects[index]->hp[1] < 1) {
		// mob 3���� ����
		for (int i = 0; i < 3; i++)
			CreateObject(x + objects[index]->size[0] / 2, y + objects[index]->size[1] / 2, objects[index]->kind + 100);

		// 9% Ȯ���� 1��° ���� drop
		if (item_code >= 90)
			CreateObject(x + objects[index]->size[0] / 2 - 2, y, 1);

		// 4% Ȯ���� 2��° ���� drop
		if (item_code <= 3)
			CreateObject(x + objects[index]->size[0] / 2 - 2, y, 2);

		// ����ġ ++
		character.exp[1] += objects[index]->exp;

		// mob object ����
		RemoveObject(index);
		return;
	}
	
	// hp ǥ�� �ð� -> �ǰ� �� 2�ʵ���
	if (objects[index]->tick[0] + 2000 > tick)
		DrawNumber(x + objects[index]->size[0] / 2 - NumLen(objects[index]->hp[1]) / 2, y - 1, objects[index]->hp[1]);

	// attack�� TRUE �̰� ���Ⱑ mob�� �浹�Ͽ��� ���
	if (character.motion[2] == 1 && CollisionCheck(objects[index]->coord, at_coord, objects[index]->size, at_size)) {
		// ���� �ð��� hp ǥ�� �ð� tick���� ����
		objects[index]->tick[0] = tick;
		// mob�� ���� ü���� user�� power�� ����
		objects[index]->hp[1] -= character.power;
		// y ���Ӱ��� -0.55 -> ü��
		objects[index]->accel[1] = -0.55;

		// 3���� ������ ��� ü���� �ѹ� �� ����
		if (character.motion[3] == 3)
			objects[index]->hp[1] -= character.power;

		// user�� �ٶ󺸰� �ִ� ������ �������� ���, x ���Ӱ��� -0.75�� ���� -> �������� ƨ�ܳ���
		if (EnemyPosition(x, objects[index]->size[0]))
			objects[index]->accel[0] = -0.75;
		// ������ ���, x ���Ӱ��� 0.75�� ���� -> ���������� ƨ�ܳ���
		else
			objects[index]->accel[0] = 0.75;
	}

	// mob kind�� 100�� ��� -> mob id�� 100�� ���
	if (objects[index]->kind == 100) {
		// y��ǥ�� �ٴ��� ��� motion[0]�� 0����
		if (y + objects[index]->size[1] == FLOOR_Y)
			objects[index]->motion[0] = 0;
		// �ƴϸ� 1��
		else
			objects[index]->motion[0] = 1;

		// mob ������ ����
		if (objects[index]->tick[1] + objects[index]->tick[2] < tick) {
			objects[index]->tick[1] = tick;
			objects[index]->tick[2] = 1000 + rand() % 1000;
			// y ���Ӱ��� random�ϰ� ������ ( �̶�, ������ x ���� ���� ������ �Ǿ�� ���������� ������ )
			objects[index]->accel[1] = rand() / (float)RAND_MAX / 2 - 1.2;

			// user�� �ٶ󺸰� �ִ� mob�� ������ �������� ���, x ���� ���� ��� random������ ������ -> ���������� ���� ũ��� ����
			if (EnemyPosition(x, objects[index]->size[0]))
				objects[index]->accel[0] = 2.4 - rand() / (float)RAND_MAX;
			// user�� �ٶ󺸰� �ִ� mob�� ������ ������ ���, x ���� ���� ���� random������ ������ -> �������� ���� ũ��� ����
			else
				objects[index]->accel[0] = rand() / (float)RAND_MAX - 2.4;
		}

		// user�� �ǰ� tick�� 0�� ��� && user�� mob�� �浹
		if (character.tick[5] == 0 && CollisionCheck(objects[index]->coord, character.coord, objects[index]->size, character.size)) {
			// user�� �ǰ� tick�� 100���� ����, user�� ü�� 10 ����
			character.tick[5] = 100;
			character.hp[1] -= 10;
		}

		// mob�� sprite�� draw
		DrawSprite(x, y, objects[index]->size[0], objects[index]->size[1], sprite_enemy1[objects[index]->motion[0]]);
	}

	// mob�� movement�� ����
	MoveControl(objects[index]->coord, objects[index]->accel, objects[index]->size, &objects[index]->flyTime);
}

// Score�� �÷��ִ� Particle�� ���� �Լ�
void ControlParticle(int index) {
	short x = objects[index]->coord[0], y = objects[index]->coord[1];
	short money_size[2] = { 2, 2 };
	short money_coord[2] = { x, y - 1 };

	// object�� kind�� 200�� ��� -> particle id�� 200�� ���
	if (objects[index]->kind == 200) {
		if (objects[index]->tick[1] < tick) {
			objects[index]->tick[1] = tick * 2;
			objects[index]->accel[0] = 2 - 4 * objects[index]->hp[0] / (float)RAND_MAX;
			objects[index]->accel[1] = -2 * objects[index]->hp[1] / (float)RAND_MAX;
		}

		// user�� particle�� �浹�� �߻��� ���
		if (CollisionCheck(money_coord, character.coord, money_size, character.size)) {
			// score + 100
			character.score += 100;

			// particle ����
			RemoveObject(index); 
			return;
		}

		// x, y ��ǥ�� particle draw
		EditMap(x, y - 1, '@');
	}

	// particle�� ���Ӱ��� ��ǥ���� ���Ͽ� �̵� �� ���
	MoveControl(objects[index]->coord, objects[index]->accel, objects[index]->size, &objects[index]->flyTime);
}

// object ������ ���� tick�� �°� mapData ����
void ControlObject() {
	for (int i = 0; i < OBJECT_MAX; i++) {
		// object�� ���� ���
		if (objects[i]) {
			// objects[i]�� item�� ���
			if (objects[i]->kind < 100)
				ControlItem(i);
			// objects[i]�� mob�� ���
			else if (objects[i]->kind > 99 && objects[i]->kind < 200)
				ControlEnemy(i);
			// objects[i]�� particle�� ���
			else
				ControlParticle(i);
		}
	}
}

// kind�� �з��Ͽ� Object����
void CreateObject(short x, short y, short kind) {
	int index = 0;
	Object* obj = 0;

	// objects �� �� ���� Ž��
	while (TRUE) {
		if (!objects[index])
			break;

		// �� ������ ���� ��� return
		if (index == OBJECT_MAX)
			return;

		index++;
	}

	// Object Heap �Ҵ�
	obj = (Object*)malloc(sizeof(Object));
	// Objects�� ���� Object ����
	objects[index] = obj;
	memset(obj, 0, sizeof(Object));

	// object�� �ʱ� ���� ����
	obj->kind = kind;
	obj->coord[0] = x; obj->coord[1] = y;
	obj->tick[0] = 0;

	// item�̳� particle�� ���
	if (kind < 100 || kind > 199) {
		// hp ���� rand num����
		obj->hp[0] = rand();
		obj->hp[1] = rand();
		obj->tick[1] = 0;
		obj->tick[2] = 0;
		obj->tick[3] = 0;
	}

	// mob�� ���
	if (kind > 99 && kind < 200) {
		// �ʱⰪ ���� kind - 100 => ������ �����صξ��� mob�� stat�� �ҷ���
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

// Object�� ����� �Լ�
void RemoveObject(int index) {
	// �ش� index�� object�� free
	free(objects[index]);
	// index�� objects�� ���
	objects[index] = 0;
}

// 1�� 2�� �浹 �˻� �Լ�
bool CollisionCheck(short coord1[], short coord2[], short size1[], short size2[]) {
	// 1�� x���� 2�� x������ 1�� x size�� �� �� ���� ũ�鼭
	// 1�� x���� 2�� x������ 2�� x size�� ���� �� ���� �۰�
	// 1�� y���� 2�� y������ 1�� y size�� �� �� ���� ũ�鼭
	// 1�� y���� 2�� y������ 2�� y size�� ���� �� ���� ���� ���  -> ��� �������Ѿ� ��. (x, y�� ��� ������ ��츦 ����.)
	if (coord1[0] > coord2[0] - size1[0] && coord1[0] < coord2[0] + size2[0]
		&& coord1[1] > coord2[1] - size1[1] && coord1[1] < coord2[1] + size2[1])
		return TRUE;
	else
		return FALSE;
}

// mob�� user�� ���� �ٶ󺸰� �ִ� ����
bool EnemyPosition(short x, short size_x) {
	// user�� x ��ǥ�� (mob�� x��ǥ + x�� ũ�� / 2) ���� ���� ��� -> mob�� ������ �ٶ�.
	if (character.coord[0] + 1 < x + floor(size_x / 2 + 0.5))
		return FALSE;
	// �ƴϸ� mob�� �������� �ٶ�.
	else
		return TRUE;
}

// x, y��ǥ���� x, y���Ӱ��� �̿��Ͽ� �������� �����ϴ� �Լ�
void MoveControl(short coord[], float accel[], short size[], float* flyTime) {
	// ���� x, y���Ӱ� -> x, y ��ǥ �̵���
	float x_value = accel[0], y_value = accel[1];

	// y��ǥ�� yũ�⸦ ���� ���� �ٴ��� ��ǥ�� ���� ��� -> �ٴڿ� ����. -> ü���ð� == 0
	if (coord[1] + size[1] == FLOOR_Y) {
		*flyTime = 0;
	}
	// �ٸ���� -> ���߿� �� ����.
	else {
		// flyTime�� ������Ű�� y���Ӱ����� flyTime ���� ����
		// �̶�, ���߿� ���� ǥ���ϱ� ���Ͽ� y���Ӱ��� ���� ���� �ִµ� �� �ð��� 0.05�� ������
		// ü�� �ð��� ���� �� ����.
		*flyTime += 0.05;
		accel[1] += *flyTime;
	}

	// x���Ӱ��� y���Ӱ��� 0�� �ƴҰ�� -> �����̰� ���� ���
	if (x_value != 0 || y_value != 0) {
		// x��ǥ�� + x ���Ӱ��� 1���� ���� ��� -> x �ּ� ���� ��� ��� -> x���Ӱ��� x �ּҰ��� ���� �ʰ�(x_value > 1)
		if (coord[0] + x_value < 1)
			x_value = 1 - coord[0];
		// x��ǥ�� + xũ�Ⱚ + x���Ӱ��� X �ִ밪���� Ŭ ��� -> x���Ӱ��� x �ִ밪�� ���� �ʰ�(x_value < MAP_X_MAX)
		if (coord[0] + size[0] + x_value > MAP_X_MAX)
			x_value = MAP_X_MAX - size[0] - coord[0];
		// y��ǥ�� + yũ�Ⱚ + y���Ӱ��� �ٴ� ���� Ŭ ��� -> y���Ӱ��� ����
		if (coord[1] + size[1] + y_value > FLOOR_Y)
			y_value = FLOOR_Y - coord[1] - size[1];
	}

	// x��ǥ���� ���Ӱ��� ���ؼ� ����. y��ǥ���� ���Ӱ��� ���ؼ� ����. -> user�� object�� x, y���� �ٲ� -> �̵�
	// ����, x_value�� ����� ��� ����������, ������ ��� �������� �̵�.
	coord[0] += floor(x_value + 0.5); coord[1] += floor(y_value + 0.5);

	// x���Ӱ��� 0���� Ŭ ��� -0.2, ���� ��� +0.2
	if (accel[0] > 0) accel[0] -= 0.2; if (accel[0] < 0) accel[0] += 0.2;
	// y���Ӱ��� 0���� Ŭ ��� -0.1, ���� ��� +0.1
	if (accel[1] > 0) accel[1] -= 0.1; if (accel[1] < 0) accel[1] += 0.1;
}

// box�� �׸��� �Լ�
void DrawBox(short x, short y, short size_x, short size_y) {
	// �ֻ�� ��, �� ������
	EditMap(x, y, '.'); EditMap(x + size_x - 1, y, '.');
	// ���ϴ� ��, �� ������
	EditMap(x, y + size_y - 1, '\''); EditMap(x + size_x - 1, y + size_y - 1, '\'');

	// ������ �׸���
	for (int i = 1; i < size_x - 1; i++) {
		EditMap(x + i, y, '-'); EditMap(x + i, y + size_y - 1, '-');
	}
	// ������ �׸���
	for (int i = 1; i < size_y - 1; i++) {
		EditMap(x, y + i, '|'); EditMap(x + size_x - 1, y + i, '|');
	}
}

// ���ڸ� draw �ϴ� �Լ�
void DrawNumber(short x, short y, int num) {
	// tmp ���� ���� �ӽ� ����, len ���� ���̰� ���� ����, cnt �ݺ��� iterator
	int tmp = num, len = NumLen(tmp), cnt = len;
	// ���� -> ���� ����� ���� ����
	char* str = malloc(sizeof(char) * len);
	memset(str, 0, sizeof(char) * len);

	do {
		// ���� ���� 1�� ���ҽ�Ű�鼭
		cnt--;
		// ascii code�� 1�� �ڸ����� ���ʴ�� ����.
		str[cnt] = (char)(tmp % 10 + 48);
		tmp /= 10;
	} while (tmp != 0);

	// x, y ��ǥ�� ���ڸ� draw
	DrawSprite(x, y, len, 1, str);
	free(str);
}

// sprite�� draw�ϴ� �Լ�
void DrawSprite(short x, short y, short size_x, short size_y, const char spr[]) {
	for (int i = 0; i < size_y; i++) {
		for (int n = 0; n < size_x; n++)
			// x�� y�� �������� n, i�� ������Ű�鼭 �޾ƿ� spr�� ��� ������ mapData�� edit
			EditMap(x + n, y + i, spr[i * size_x + n]);
	}
}

// ������ str�迭�� max_value��ŭ str_s�� ���� ä��
void FillMap(char str[], char str_s, int max_value) {
	for (int i = 0; i < max_value; i++)
		str[i] = str_s;
}

// mapData�迭���� x, y �� ��ȿ�� ��ǥ�� ���, str�� mapData�� edit
void EditMap(short x, short y, char str) {
	if (x > 0 && y > 0 && x - 1 < MAP_X_MAX && y - 1 < MAP_Y_MAX)
		mapData[(y - 1) * MAP_X_MAX + x - 1] = str;
}

// ������ ���̸� ���ϴ� �Լ�
int NumLen(int num) {
	int tmp = num, len = 0;

	// 0�̸� ���̰� 1�̹Ƿ� 1�� return
	if (num == 0) {
		return 1;
	}
	else {
		// tmp�� 10���� �����鼭 len�� ������Ŵ -> �ڸ������� len ����
		while (tmp != 0) {
			tmp /= 10;
			len++;
		}
	}

	return len;
}