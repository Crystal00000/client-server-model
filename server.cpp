#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <stdio.h>
#include <algorithm>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <errno.h>
#include <map>
#include <cstdlib>
#include <string>
#include <ctype.h>
#include <signal.h>

using namespace std;
#define MAXLINE 1024

class TCP;
class ROOM;
class GAME;
class USR_INFO {
public:
	string name;
	string email;
	string password;
	string room_id;
	bool login = 0;
	int conn;
	map<string, ROOM*> invs;

	bool inv_exist(string inviter_email);
	bool inv_code_correct(string room_id, unsigned long inv_code);
	// void leave_room();
	void Login(int conn);
	void Logout();
};

class ROOM {
public:
	string ID;
	USR_INFO* manager;
	bool pblc, playing;
	unsigned long inv_code;
	vector<USR_INFO*> players;
	GAME* game = NULL;

	void start();
	void stop();
	void leave(USR_INFO* player, map<string, ROOM*>& rooms);
	void delete_room();
};

bool USR_INFO::inv_exist(string inviter_email) {
	map<string, ROOM*>::iterator inv_it = this->invs.begin();
	cout << name << "'s invs map size: " << invs.size() << "\n";
	for (inv_it; inv_it != this->invs.end(); ++inv_it) {
		string tmp = inv_it->second->manager->email;
		cout << tmp << ", conn = " << inv_it->second->manager->conn << "\n";
		if (tmp == inviter_email) return true;					
	}
	cout << "return false\n";
	return false;
}
bool USR_INFO::inv_code_correct(string room_id, unsigned long inv_code) {
	map<string, ROOM*>::iterator inv_it = this->invs.find(room_id);
	return (inv_it->second->inv_code == inv_code);
}
void USR_INFO::Login(int conn){
	this->conn=conn;
	this->login=1;
}
void USR_INFO::Logout(){
	this->conn=-1;
	this->login=0;
}


void ROOM::start() {

}
void ROOM::stop() {
	// have to do somthing here?!
	playing = false;
}
void ROOM::leave(USR_INFO* player, map<string, ROOM*>& rooms) {
	string tmp_room_id = player->room_id;
	// player->invs.erase(player->room_id);
	player->room_id = "";
	cout << "before remove, map size = " << players.size() << "\n";
	// players.erase(players.find(player));
	players.erase(remove(players.begin(), players.end(), player), players.end());
	cout << "after remove, map size = " << players.size() << "\n";

	if (playing) stop();
	if (players.empty()) {
		cout << "players is empty, FREE\n";
		// free(this);
		rooms.erase(tmp_room_id);
	}
}



void* Handle(void *data);
typedef void * (*THREADFUNCPTR)(void *);

pthread_t threads[50];
int threadno = 0, TCPsock;

vector<string> split (const string &s, char delim) {
	vector<string> result;
    stringstream ss (s);
    string item;

    while (getline (ss, item, delim)) {
		result.push_back (item);
    }
    return result;
}


struct ARGS {
    int argc;
    const char **argv;
};
ARGS args;

struct CONN_DATA{
	TCP* cptr;
	int conn;
};

class GAME {
public:
	ROOM* room;

	char ans[5] = {0};
	char buf[MAXLINE] = {0};
	int round;
	int cur_guesser_idx;

	GAME (ROOM* room, int round) {
		this->room = room;
		this->round = round;
		this->cur_guesser_idx = 0;
	}
	void setRandomAns() {
		for (int i = 0; i < 4; ++i) {
			ans[i] = (rand() % 10) + '0';
		}
	}
	bool setCustomAns(string str_num) {
		if (is_valid(str_num)) {
			strcpy(ans, str_num.c_str());
			// printSetAns();
			return true;
		} else {
			return false;
		}
	}
	void printSetAns() {
		cout << "Set answer as ";
		for (int i = 0; i < 4; ++i) {
			cout << ans[i] << " ";
		}
		cout << "\n";
	}
	bool is_valid(string guess) {
		if ((guess.size() != 4)) {
			return false;
		}
		if (!isdigit(guess[0]) || !isdigit(guess[1]) ||
			!isdigit(guess[2]) || !isdigit(guess[3])) {
			return false;
		}
		return true;
	}
	
	string check(string str_guess) {
		int a{0}, b{0};
		char guess[5];
		strcpy(guess, str_guess.c_str());

		for (int i = 0; i < 4; ++i) {
			if (ans[i] == guess[i]) ++a;
		}

		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				if (ans[i] == guess[j]) {
					++b;
					guess[j] = 'Z';
					break;
				}
			}
		}
		b -= a;

		string response = to_string(a) + "A" + to_string(b) + "B";
		
		return response;
	}
	string guess(USR_INFO* player, string guess) {
		cout << "round == " << round << "\n";
		string response =  check(guess);

		cur_guesser_idx++;
		if (response == "4A0B") { // bingo
			response = player->name + " guess '" + guess + "' and got Bingo!!! " + player->name + " wins the game, game ends";
			this->room->playing = false;
			cout << "this->room->playing is set to false!\n";
		} else if (round == 1 && (cur_guesser_idx == this->room->players.size())) { // no chance			
			response = player->name + " guess '" + guess + "' and got '" + response + "'\nGame ends, no one wins";		
			this->room->playing = false;
			cout << "this->room->playing is set to false!\n";
		} else { // no bingo
			response = player->name + " guess '" + guess + "' and got '" + response + "'";
			if (cur_guesser_idx == this->room->players.size()) {
				cur_guesser_idx = 0;
				--round;
			}
			
		}
		
		
		return response;
	}
	string start() { // to be partitioned
		while (round--) {
			for (auto player_it : room->players) {
			}
		}
		/*
		while (chance != 0) {
			memset(buf,0,sizeof(buf));
			int n = recv(connfd, buf, MAXLINE, 0);
			buf[n] = '\0';
			
			if(!is_valid(buf)) {
				response = "Your guess should be a 4-digit number.";
			} else {
				--chance;
				response = check(buf);
			}
			if (response == "You got the answer!") {
				return response;
			} else {
				if (chance == 0) {
					response = response + "\nYou lose the game!";
					return response;
				}
				memset(buf, 0, sizeof(buf));
				strcpy(buf, response.c_str());	
				if ((send(connfd, buf, sizeof(buf), 0)) < 0) {
					printf("\tTCP sends response failed\n");
				}
			}
		}
		*/
	}
};

class MAINSERVER{
public:
	map<string, USR_INFO*> usrname;
	map<string, USR_INFO*> usremail;
	map<string, ROOM*> rooms;
	
	string gm_rl(string recv);
	string regst(string recv);
	string ls_rooms();
	string ls_users();
	string login(string recv, string &cur_username, int conn);
	string logout(string recv, string &cur_username, int conn);
	string create_public(string recv, string &cur_username, int conn);
	string create_private(string recv, string &cur_username, int conn);
	string startgame(string recv, string &cur_username);
	string guess(string recv, string &cur_username);
	string join(string recv, string &cur_username, int conn);
	string leave(string recv, string &cur_username, int conn);
	string invite(string recv, string &cur_username, int conn);
	string ls_invs(string recv, string &cur_username, int conn);
	string accept(string recv, string &cur_username, int conn);
	void broadcast(int conn, string msg);
	void leave_by_exit(string cur_username, int conn);
};
void MAINSERVER::leave_by_exit(string cur_username, int conn){
	map<string, USR_INFO*>::iterator user_it;
	user_it = usrname.find(cur_username);
	cout<<"leaver: "<<cur_username<<"\n";
	if (cur_username != "" && user_it->second->room_id.size()) {
		map<string, ROOM*>::iterator room_it;
		room_it = rooms.find(user_it->second->room_id);
		
		if (user_it->second == room_it->second->manager) { // is manamger
			string tmp_room_id = user_it->second->room_id;
			room_it->second->leave(user_it->second, rooms);
			for (auto player_it : room_it->second->players) {								
				room_it->second->leave(player_it, rooms);
			}			
		} else {
			room_it->second->leave(user_it->second, rooms);			
		} 
	}			
}
string MAINSERVER::ls_rooms(){
	string room_list = "List Game Rooms";
	if (rooms.empty()) {
		room_list += "\nNo Rooms";
	} else {
		map<string, ROOM*>::iterator room_it;
		int c = 1;
		for (room_it = rooms.begin(); room_it != rooms.end(); room_it++) {
			room_list = room_list + "\n" + to_string(c) + ".";
			room_list += (room_it->second->pblc) ? " (Public) " : " (Private) ";
			room_list += "Game Room ";
			room_list += room_it->first;
			room_list += (room_it->second->playing) ? " has started playing" : " is open for players";
			++c;
		}				
	}
	return room_list;
}
string MAINSERVER::ls_users(){
	string user_list = "List Users";
	if (usrname.empty()) {
		user_list += "\nNo Users";
	} else {
		map<string, USR_INFO*>::iterator user_it;
		int c = 1;
		for (user_it = usrname.begin(); user_it != usrname.end(); user_it++) {
			user_list = user_list + "\n" + to_string(c) + ". "
			 + user_it->second->name + "<" + user_it->second->email + ">";
			user_list += (user_it->second->login) ? " Online" : " Offline";
			++c;
		}				
	}
	return user_list;
}
string MAINSERVER::ls_invs(string recv, string &cur_username, int conn){
	string inv_list = "List invitations";
	map<string, USR_INFO*>::iterator user_it;
	user_it = usrname.find(cur_username);
		
	if (user_it->second->invs.empty()) {
		inv_list += "\nNo Invitations";
	} else {
		int c = 1;
		map<string, ROOM*>::iterator inv_it = user_it->second->invs.begin();
		for (inv_it; inv_it != user_it->second->invs.end(); ++inv_it) {
			if(rooms.find(inv_it->first)==rooms.end()){
				continue;
			}
			inv_list = inv_list + "\n" + to_string(c) + ". "
			 + inv_it->second->manager->name + "<" + inv_it->second->manager->email + 
			"> invite you to join game room "
			 + inv_it->second->ID + 
			", invitation code is "
			 + to_string(inv_it->second->inv_code);
			++c;
		}				
	}
	return inv_list;
}
string MAINSERVER::gm_rl(string recv){
	string game_rule;
	game_rule = "1. Each question is a 4-digit secret number.\n2. After each guess, you will get a hint with the following information:\n2.1 The number of \"A\", which are digits in the guess that are in the correct position.\n2.2 The number of \"B\", which are digits in the guess that are in the answer but are in the wrong position.\nThe hint will be formatted as \"xAyB\".\n3. 5 chances for each question.";
	return game_rule;
}
string MAINSERVER::regst(string recv){
	vector<string> cmd = split (recv, ' ');
	string response;
	
	if (cmd.size() != 4) {
		response = "Usage: register <username> <email> <password>";
	} else {
		bool flg = 1;

		map<string, USR_INFO*>::iterator n_it;
		map<string, USR_INFO*>::iterator e_it;
		n_it = usrname.find(cmd[1]);
		e_it = usremail.find(cmd[2]);
		if (n_it != usrname.end()) { // name exist
			response = "Username or Email is already used";
			flg = 0;
		} else { // name not exist
			if (e_it != usremail.end()) { //email exist
				response = "Username or Email is already used";
				flg = 0;
			}
		}
		if (flg) {
			USR_INFO* tmp = new USR_INFO{.name = cmd[1], .email = cmd[2], .password = cmd[3]};
			usrname[cmd[1]] = tmp;
			usremail[cmd[2]] = tmp;
			response = "Register Successfully";
		}	
	}
	return response;

}
string MAINSERVER::login (string recv, string &cur_username, int conn) {
	vector<string> cmd = split (recv, ' ');
	string response;

	if (cmd.size() != 3) {
		response = "Usage: login <username> <password>";
	} else {
		map<string, USR_INFO*>::iterator n_it;
		n_it = usrname.find(cmd[1]);
		
		if (n_it != usrname.end()) { // name exist
			if (!(usrname[cmd[1]]->password == cmd[2])) {  // check password
				response = "Wrong password";					
			} else if (cur_username != "") { // check login status
				response = "You already logged in as " + cur_username;
			} else if (usrname[cmd[1]]->login) {
				response = "Someone already logged in as " + cmd[1];
			} else {
				response = "Welcome, " + cmd[1];
				usrname[cmd[1]]->Login(conn);
				// usrname[cmd[1]]->login = 1;
				// usrname[cmd[1]]->conn = conn;
				cur_username = cmd[1];

			}
		} else {
			response = "Username does not exist";
		}
	}
	return response;
}
string MAINSERVER::logout(string recv, string &cur_username, int conn) {
	vector<string> cmd = split (recv, ' ');
	string response;

	if (cmd.size() != 1) {
		response = "Usage: logout";
	} else { // have to know name first
		map<string, USR_INFO*>::iterator n_it;
		n_it = usrname.find(cur_username);
			
		if(n_it->second->room_id != "") {
			response="You are already in game room "+n_it->second->room_id+", please leave game room";
		}else{
			if (n_it != usrname.end()) { // name exist
				// usrname[cur_username]->login = 0;
				usrname[cur_username]->Logout();
				response = "Goodbye, " + cur_username;
				cur_username = "";
			} else {
				response = "You are not logged in";
			}
		}
	}
	return response;
}
string MAINSERVER::create_public(string recv, string &cur_username, int conn){
	vector<string> cmd = split (recv, ' ');
	string response;

	if (cmd.size() != 4) {
		response = "Usage: create public room <game room id>";
	} else {
		if (cur_username == "") {
			// cout << "cur_username is \"\"\n";
			response = "You are not logged in";
		} else {
			// cout << "cur_username is " << cur_username << "\n";
			map<string, ROOM*>::iterator room_it;
			room_it = rooms.find(cmd[3]);	
			map<string, USR_INFO*>::iterator user_it;
			user_it = usrname.find(cur_username);
			
			if (user_it->second->room_id == "") {  // check whether in a room
				if ((room_it == rooms.end()) || (room_it->second == NULL)) { // ID available
					user_it->second->room_id = cmd[3];
					ROOM* tmp = new ROOM{.ID = cmd[3], .manager = user_it->second, .pblc = true, .playing = false, .inv_code = 0};
					tmp->players.push_back(user_it->second);
					rooms[cmd[3]] = tmp;

					response = "You create public game room " + cmd[3];	
					
				} else {
					response = "Game room ID is used, choose another one";
				}
			} else {
				response = "You are already in game room " + user_it->second->room_id + ", please leave game room";
			}
		}	
	}
	return response;
}
string MAINSERVER::create_private(string recv, string &cur_username, int conn){
	vector<string> cmd = split (recv, ' ');
	string response;

	if (cmd.size() != 5) {
		response = "Usage: create private room <game room id> <invitation code>";
	} else {
		if (cur_username == "") {
			// cout << "cur_username is \"\"\n";
			response = "You are not logged in";
		} else {
			// cout << "cur_username is " << cur_username << "\n";
			map<string, ROOM*>::iterator room_it;
			room_it = rooms.find(cmd[3]);	
			map<string, USR_INFO*>::iterator user_it;
			user_it = usrname.find(cur_username);

			if (user_it->second->room_id == "") {  // check whether in a room
				if (room_it == rooms.end()) { // ID available
					user_it->second->room_id = cmd[3];
					ROOM* tmp = new ROOM{.ID = cmd[3], .manager = user_it->second, .pblc = false, .playing = false, .inv_code = stoul(cmd[4], nullptr, 0)};
					tmp->players.push_back(user_it->second);
					rooms[cmd[3]] = tmp;
					response = "You create private game room " + cmd[3];
				} else {
					response = "Game room ID is used, choose another one";
				}		
			} else {
				response = "You are already in game room " + user_it->second->room_id + ", please leave game room";
			}
			
			
		}	
	}
	return response;
}
string MAINSERVER::startgame(string recv, string &cur_username) {
	vector<string> cmd = split (recv, ' ');
	string response;

	if (cmd.size() != 3 && cmd.size() != 4) {
		response = "Usage: start game <number of rounds> <guess number>";
	} else {
		if (cur_username == "") {
			// cout << "cur_username is \"\"\n";
			response = "You are not logged in";
		} else {
			// cout << "cur_username is " << cur_username << "\n";
			map<string, USR_INFO*>::iterator user_it;
			user_it = usrname.find(cur_username);
			map<string, ROOM*>::iterator room_it;
			room_it = rooms.find(user_it->second->room_id);
			if (!user_it->second->room_id.size()) {
				response = "You did not join any game room";
			} else {
				if ((room_it->second->manager->name != cur_username)) {
					response = "You are not game room manager, you can't start game";
				} else {
					if (room_it->second->playing) {
						response = "Game has started, you can't start again";
					} else {
						GAME* game = new GAME(room_it->second, stoi(cmd[2]));
						room_it->second->game = game;
						if (cmd.size() == 3) { // RandomAns
							game->setRandomAns();							
						} else { // CustomAns
							if (game->setCustomAns(cmd[3]) == 0) { // if fail
								return response = "Please enter 4 digit number with leading zero";
							}
						}
						
						response = "Game start! Current player is "+ room_it->second->players[room_it->second->game->cur_guesser_idx]->name;
						
						for (auto player_it : room_it->second->players) {
							if(player_it->name==cur_username) continue;
							broadcast(player_it->conn, response);
						}
						room_it->second->playing = true;
						room_it->second->players[room_it->second->game->cur_guesser_idx]->name = cur_username;
						
						// game->start();			
					}
					
				}
			}
		}	
	}
	return response;
	
}
string MAINSERVER::guess(string recv, string &cur_username) {
	vector<string> cmd = split (recv, ' ');
	string response;

	if (cmd.size() != 2) {
		response = "Usage: guess <guess number>";
	} else {
		if (cur_username == "") {
			// cout << "cur_username is \"\"\n";
			response = "You are not logged in";
		} else {
			// cout << "cur_username is " << cur_username << "\n";
			map<string, USR_INFO*>::iterator user_it;
			user_it = usrname.find(cur_username);
			map<string, ROOM*>::iterator room_it;
			room_it = rooms.find(user_it->second->room_id);
			if (!user_it->second->room_id.size()) {
				response = "You did not join any game room";
			} else {
				if (!room_it->second->playing) { // haven't started
					if ((room_it->second->manager->name == cur_username)) {
						response = "You are game room manager, please start game first";
					} else {
						response = "Game has not started yet";
					}
				} else { // started
					cout << "room_it->second->game->cur_guesser_idx = " << room_it->second->game->cur_guesser_idx << "\n";
					if (room_it->second->players[room_it->second->game->cur_guesser_idx]->name != cur_username) {
						response = "Please wait..., current player is " + room_it->second->players[room_it->second->game->cur_guesser_idx]->name;
					} else {
						if (!room_it->second->game->is_valid(cmd[1])) {
							response = "Please enter 4 digit number with leading zero";
						} else {
							response = room_it->second->game->guess(user_it->second, cmd[1]);
							for (auto player_it : room_it->second->players) {
								if (player_it == user_it->second) continue;								
								broadcast(player_it->conn, response);
							}
						}
					}
				}
				
			}
		}	
	}
	return response;
}

string MAINSERVER::join(string recv, string &cur_username, int conn){
	vector<string> cmd = split (recv, ' ');
	string response;

	if (cmd.size() != 3) {
		response = "Usage: join room <game room id>";
	} else {
		if (cur_username == "") {
			// cout << "cur_username is \"\"\n";
			response = "You are not logged in";
		} else {
			// cout << "cur_username is " << cur_username << "\n";
			map<string, ROOM*>::iterator room_it;
			room_it = rooms.find(cmd[2]);
			map<string, USR_INFO*>::iterator user_it;
			user_it = usrname.find(cur_username);

			if (user_it->second->room_id != "") {  // check whether in a room
				response = "You are already in game room " + user_it->second->room_id + ", please leave game room";
			} else {
				if (room_it == rooms.end()) { // check ID existence
					response = "Game room " + cmd[2] + " is not exist";
				} else {
					if (!room_it->second->pblc) { // check "public"
						response = "Game room is private, please join game by invitation code";
					} else { // check room status
						if (room_it->second->playing) {
							response = "Game has started, you can't join now";
						} else {
							user_it->second->room_id = cmd[2];

							for (auto tmp_player : room_it->second->players) {								
								string msg = "Welcome, "+ cur_username + " to game!";
								broadcast(tmp_player->conn, msg);
							}
							room_it->second->players.push_back(user_it->second);
							response = "You join game room " + cmd[2];
						}
					}
				}	
			}
		}	
	}
	return response;
}
string MAINSERVER::leave(string recv, string &cur_username, int conn){
	vector<string> cmd = split (recv, ' ');
	string response;

	if (cmd.size() != 2) {
		response = "Usage: leave room";
	} else {
		if (cur_username == "") {
			// cout << "cur_username is \"\"\n";
			response = "You are not logged in";
		} else {
			map<string, USR_INFO*>::iterator user_it;
			user_it = usrname.find(cur_username);
			if (!user_it->second->room_id.size()) {
				response = "You did not join any game room";
			} else {
				map<string, ROOM*>::iterator room_it;
				room_it = rooms.find(user_it->second->room_id);
				if (user_it->second == room_it->second->manager) { // is manamger
					string tmp_room_id = user_it->second->room_id;
					response = "You leave game room " + tmp_room_id;
					room_it->second->leave(user_it->second, rooms);
					for (auto player_it : room_it->second->players) {								
						string msg = "Game room manager leave game room " + tmp_room_id + ", you are forced to leave too";
						broadcast(player_it->conn, msg);
						room_it->second->leave(player_it, rooms);
					}
					
				} else {
					if (room_it->second->playing) { // playing
						string tmp_room_id = user_it->second->room_id;
					
						response = "You leave game room " + tmp_room_id + ", game ends";
						room_it->second->leave(user_it->second, rooms);
						
						for (auto player_it : room_it->second->players) {								
							string msg = cur_username + " leave game room " + tmp_room_id + ", game ends";
							broadcast(player_it->conn, msg);
						}
						
					} else { //not playing
						string tmp_room_id = user_it->second->room_id;
					
						response = "You leave game room " + tmp_room_id;
						room_it->second->leave(user_it->second, rooms);
						for (auto player_it : room_it->second->players) {								
							string msg = cur_username + " leave game room " + tmp_room_id;
							broadcast(player_it->conn, msg);
						}
						
					}
				} 
			}			
		}	
	}
	return response;
}
string MAINSERVER::invite(string recv, string &cur_username, int conn){
	vector<string> cmd = split (recv, ' ');
	string response;

	if (cmd.size() != 2) {
		response = "Usage: invite <invitee email>";
	} else {
		if (cur_username == "") {
			cout << "cur_username is \"\"\n";
			response = "You are not logged in";
		} else {
			cout << "cur_username is " << cur_username << "\n";
			
			map<string, USR_INFO*>::iterator user_it;
			user_it = usrname.find(cur_username);
			
			map<string, ROOM*>::iterator room_it;
			room_it = rooms.find(user_it->second->room_id);	

			map<string, USR_INFO*>::iterator invitee_it;
			invitee_it = usremail.find(cmd[1]);

			if (user_it->second->room_id == "") {  // check whether in a room
				response = "You did not join any game room";
			} else {
				if ((room_it->second->manager->name != cur_username) || (room_it->second->pblc)) { // check whether is manager
					response = "You are not private game room manager";
				} else {
					if (!invitee_it->second->login) { // check invitee
						response = "Invitee not logged in";
					} else { // success
						// // send msg to invitee
						string tmp = "You receive invitation from " + user_it->second->name + "<" + user_it->second->email + ">";
						broadcast(invitee_it->second->conn, tmp);
						// send msg to inviter
						(invitee_it->second->invs)[room_it->second->ID] = room_it->second;
						response = "You send invitation to " + invitee_it->second->name + "<" + invitee_it->second->email + ">";					
					}
				}	
			}

			
		}	
	}
	return response;
}
string MAINSERVER::accept(string recv, string &cur_username, int conn){
	vector<string> cmd = split (recv, ' ');
	string response;

	if (cmd.size() != 3) {
		response = "Usage: accept <inviter email> <invitation code>";
	} else {
		if (cur_username == "") {
			// cout << "cur_username is \"\"\n";
			response = "You are not logged in";
		} else {
			// cout << "cur_username is " << cur_username << "\n";
			// map<string, ROOM*>::iterator room_it;
			// room_it = rooms.find(cmd[2]);
			map<string, USR_INFO*>::iterator user_it;
			user_it = usrname.find(cur_username);

			if (user_it->second->room_id != "") {  // check whether in a room
				response = "You are already in game room " + user_it->second->room_id + ", please leave game room";
			} else {				
				if (!user_it->second->inv_exist(cmd[1])) { // check invitation existence
					response = "Invitation not exist";
				} else {
					map<string, USR_INFO*>::iterator inviter_it = usremail.find(cmd[1]);
					string room_id = inviter_it->second->room_id;

					if (!user_it->second->inv_code_correct(room_id, stoul(cmd[2]))) { // check code corretness
						response = "Your invitation code is incorrect";
					} else { // check room status
						if (rooms[room_id]->playing) {
							response = "Game has started, you can't join now";
						} else {
							user_it->second->room_id = room_id;
							for (auto tmp_player : rooms[room_id]->players) {								
								string msg = "Welcome, "+ cur_username + " to game!";
								broadcast(tmp_player->conn, msg);
							}
							rooms[room_id]->players.push_back(user_it->second);
							response = "You join game room " + room_id;
						}
					}
				}	
			}
		}	
	}
	return response;
}
void MAINSERVER::broadcast(int conn, string msg){
	cout << "broadcast to " << conn << ": " << msg << "\n";
	msg+="\n";
	char buf[msg.size()];
	strcpy(buf, msg.c_str());
	if ((send(conn, buf, sizeof(buf), 0)) < 0) {
		printf("\tTCP sends response failed\n");
		return;
	}
}

class TCP {
public:	
	MAINSERVER *mserver;	
	int TCPsock, connfd;
	sockaddr_in TCPservaddr;

	TCP(MAINSERVER *mp){
		this->mserver=mp;
	}

    void TCPstart(){
        // create
        if( (TCPsock = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
            printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
            return;
        }
        int opt = 1;
        if (setsockopt(TCPsock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }
        memset(&TCPservaddr, 0, sizeof(TCPservaddr));
        TCPservaddr.sin_family = AF_INET;
        TCPservaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        TCPservaddr.sin_port = htons(8888); //(uint16_t)atoi(args.argv[1])
        // bind
        if( bind(TCPsock, (struct sockaddr*)&TCPservaddr, sizeof(TCPservaddr)) == -1){
            printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
            return;
        }
        if( listen(TCPsock, 10) == -1){
            printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
            return;
        }
		cout << "TCP server is running\n";
		TCPcommunicate();
		TCPclose();
    }
    void TCPcommunicate() {
        while (1) {
            if( (connfd = accept(TCPsock, (struct sockaddr*)NULL, NULL)) == -1){
                printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            }
			sleep(1);
            cout << "New connection.\n";
			CONN_DATA d;
			d.cptr = this;
			d.conn = connfd;
			pthread_create(&threads[threadno++], NULL, Handle, &d);
            
        }
    }
    void TCPclose() {
        
        close(TCPsock);
    }
	void handle(int connfd){ // each thread
		signal(SIGPIPE, SIG_IGN);
		
		string cur_username = ""; //for accessing the map in mp

		char buf[MAXLINE] = {0};
		string welMsg = "*****Welcome to Game 1A2B*****";
		memset(buf, 0, sizeof(buf));
		strcpy(buf, welMsg.c_str());
		// if ((send(connfd, buf, sizeof(buf), 0)) < 0) {
		// 	printf("\tTCP sends response failed\n");
		// 	return;
		// }
		while(1){
			memset(buf,0,sizeof(buf));
			int n = recv(connfd, buf, MAXLINE, 0);
			buf[strlen(buf)-1]='\0';
			cout<<"tcp recv: "<<buf<<";\n";

			if (string(buf) == "exit" || string(buf) == "") {
				cout << "TCP gets EXIT\n";
				if(cur_username.size())
					this->mserver->leave_by_exit(cur_username, connfd);
					
					this->mserver->logout(string("logout"), cur_username, connfd);
				break;
			}
			
			string response;
			vector<string> cmd = split (buf, ' ');
			if (cmd[0] == "login") { 
				response = this->mserver->login(string(buf), cur_username, connfd);
			} else if (cmd[0] == "logout") {
				response = this->mserver->logout(string(buf), cur_username, connfd);
			} else if (cmd[0] == "start" && cmd[1] == "game") {
				response = this->mserver->startgame(string(buf), cur_username);
			} else if (cmd[0] == "guess") {
				response = this->mserver->guess(string(buf), cur_username);
			} else if (cmd[0] == "create" && cmd[2] == "room") { 
				if (cmd[1] == "public") {
					response = this->mserver->create_public(string(buf), cur_username, connfd);				
				} else if (cmd[1] == "private"){
					response = this->mserver->create_private(string(buf), cur_username, connfd);
				}
			} else if (cmd[0] == "join" && cmd[1] == "room") {
					response = this->mserver->join(string(buf), cur_username, connfd);
			} else if (cmd[0] == "invite") {
					response = this->mserver->invite(string(buf), cur_username, connfd);
			} else if (cmd[0] == "list" && cmd[1] == "invitations") {
					response = this->mserver->ls_invs(string(buf), cur_username, connfd);
			} else if (cmd[0] == "accept") {
					response = this->mserver->accept(string(buf), cur_username, connfd);
			} else if (cmd[0] == "leave" && cmd[1] == "room") {
					response = this->mserver->leave(string(buf), cur_username, connfd);
			} else { 
				response = "unknown command";
			}
			response+="\n";
						
			char sendbuf[response.size()];
			strcpy(sendbuf, response.c_str());
			cout<<"tcp send: "<<sendbuf<<"\n";
			if ((send(connfd, sendbuf, sizeof(sendbuf), 0)) < 0) {
				printf("\tTCP sends response failed\n");
				return;
			}
		}
		close(connfd);

	}
};

void* Handle(void *data){
   ((CONN_DATA*)data)->cptr->handle(((CONN_DATA*)data)->conn);
   pthread_exit(NULL);
}

class UDP {
public:
	MAINSERVER *mserver;
	int UDPsock;
	sockaddr_in UDPservaddr; // server address
	sockaddr_in UDPclientaddr; // client address
	socklen_t UDPaddrlen = sizeof(UDPclientaddr);
	
	UDP(MAINSERVER *mp){
		this->mserver=mp;
	}

    void* UDPstart() { 
		// create
		if ((UDPsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
			cout << "UDPsock creation failed\n\tExiting\n";
			return NULL;
		}
		int opt = 1;
		if (setsockopt(UDPsock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
			perror("setsockopt");
			exit(EXIT_FAILURE);
		}
		
		memset((sockaddr*)&UDPservaddr, 0, sizeof (UDPservaddr));
		UDPservaddr.sin_family = AF_INET;
		UDPservaddr.sin_addr.s_addr = htonl (INADDR_ANY);
		UDPservaddr.sin_port = htons (8888); //(uint16_t)atoi(args.argv[1])
		// bind
		if (bind (UDPsock, (sockaddr*)&UDPservaddr, sizeof (UDPservaddr)) == -1) {
			cout << "\nUDPsock Binding faile\n\tExiting\n";
			return NULL;
		}
		cout << "UDP server is running\n";
		UDPcommunicate();
    }
	void UDPcommunicate() {
		char buf[MAXLINE] = {0};
		while (1) {
			sleep(1);

        	memset(buf,0,sizeof(buf));
			recvfrom (UDPsock, buf, MAXLINE, 0, (sockaddr*) &UDPclientaddr, &UDPaddrlen);
		
			string response;
			vector<string> cmd = split (buf, ' ');
			if (cmd[0] == "register") { 
				response = this->mserver->regst(string(buf));
			} else if (cmd[0] == "game-rule") { 
				response = this->mserver->gm_rl(string(buf));
			} else if (cmd[0] == "list") { 
				if (cmd[1] == "rooms") {
					response = this->mserver->ls_rooms();
				} else if (cmd[1] == "users") {
					response = this->mserver->ls_users();
				} else {
					response = "unknown command";
				}
			} else { // "unknown command"
				response = "unknown command";
			}
			response+="\n";

			char sendbuf[response.size()];
			strcpy(sendbuf, response.c_str());
			sendto(UDPsock, sendbuf, strlen(sendbuf), 0, (sockaddr*)&UDPclientaddr, UDPaddrlen);
			cout<<"udp send: "<<sendbuf<<";\n";
		}
	}
	void UDPclose() {	
		close(UDPsock);
	}
};

int main() { //int argc, char const *argv[]
	srand((unsigned) time(NULL));

	// if (argc != 2){
	// 	printf("Usage: ./server <port>\n"); return 0;}

	// args.argc = argc; 
	// args.argv = argv;

	MAINSERVER *ms=new MAINSERVER();
	
	sleep(1);
	
	UDP UDPobj(ms);
	TCP TCPobj(ms);
	
	pthread_create (&threads[threadno++], NULL, (THREADFUNCPTR)&UDP::UDPstart, &UDPobj);
	pthread_create (&threads[threadno++], NULL, (THREADFUNCPTR)&TCP::TCPstart, &TCPobj);
	
	pthread_join(threads[0], NULL);
	pthread_join(threads[1], NULL);	
	
	free(ms);
	return 0;
}