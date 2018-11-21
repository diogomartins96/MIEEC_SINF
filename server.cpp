#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <list>
#include <sstream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <postgresql/libpq-fe.h>

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include <pthread.h>


#define START_BALANCE	200

#define ANONYMOUS 		0
#define VISITOR			1
#define USER			2
#define ADMIN			3


using namespace std;

struct User {
	string username;
	int id;
	int rule;	
};

struct Player {
	int id;
	int balance;
	int bet;
};

struct Runner {
	string name;
	int id;
	int speed;
	int performance;
	int total_bet;
	
	vector<Player *> players;
	
	int place;
	int points;
};

int serve_sockfd;
map<int, User*> clients;
PGconn* db_conn = NULL;
pthread_mutex_t mutex;

vector<string> split(const string &s, char delim) {
    vector<string> elems;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

/* Envia uma string para um socket */
void writeline(int socketfd, string line) {
	string tosend = line + "\n";
	write(socketfd, tosend.c_str(), tosend.length());
}

/* Lê uma linha de um socket
   retorna false se o socket se tiver fechado */
bool readline(int socketfd, string &line) {
  int n; 
  char buffer[1025]; 
  line = "";

  while (line.find('\n') == string::npos) {

    int n = read(socketfd, buffer, 1024);
    if (n == 0) return false;
    buffer[n] = 0;
    line += buffer;
  }

  line.erase(line.end() - 1);
  line.erase(line.end() - 1);
  return true;  
}

/* Envia uma mensagem para todos os clientes ligados exceto 1 */
void broadcast (int origin, string text) {

   map<int, User* >::iterator it;
   for (it = clients.begin(); it != clients.end(); it++)
     if (it->first != origin) writeline(it->first, text);
}


void multicast(vector<int> socketfds, string text) {
	
	for(int i = 0; i < socketfds.size(); i++)
		writeline(socketfds[i], text);
}


//DataBase
void initDB() {
	
	const char db_conninfo[] = "host='dbm.fe.up.pt' user='sinf16g23' password='sinf16g23' dbname='sinf16g23'";
	
	db_conn = PQconnectdb(db_conninfo);

  if(!db_conn) {
	  cout << "Failed to connect to database (1)" << endl;
	  exit(-1);
  }
 
  if(PQstatus(db_conn) != CONNECTION_OK) {
	  cout << "Failed to connect to database (2)" << endl;
	  exit(-1);
  }
}

void closeDB() {
	
	PQfinish(db_conn);
}

PGresult* executeSQL(string sql)
{
	PGresult* result = PQexec(db_conn, sql.c_str());
 
	if (!(PQresultStatus(result) == PGRES_COMMAND_OK || PQresultStatus(result) == PGRES_TUPLES_OK))
	{
		cout << "Failed to execute sql" << endl;
		return NULL;
	}
 
	return result;
}

int getSockfdByUserId(int id) {
	
	map<int, User* >::iterator it;

	for(it = clients.begin(); it != clients.end(); it++)
		if(it->second->id == id)
			return it->first;
	
	return -1;
}

int getSockfdByUsername(string username) {
	
	map<int, User* >::iterator it;

	for(it = clients.begin(); it != clients.end(); it++)
		if(it->second->username == username)
			return it->first;
	
	return -1;
}

//Commands

void helpCmd(int sockfd) {
	
	ostringstream msg;
	
	msg << "> Commands:" << endl << endl;
	msg << "	\\help" << endl;
	msg << "	\\register <username> <password>" << endl;
	msg << "	\\identify |<username>|" << endl;
	msg << "	\\login <username> <password>" << endl;
	msg << "	\\logout" << endl;
	msg << "	\\add <horse> <speed>" << endl;
	msg << "	\\create <description> <number> <laps>" << endl;
	msg << "	\\bet <raceId> <horse> <value>" << endl;
	msg << "	\\start <raceId>" << endl;
	msg << "	\\log <horse> <limit>" << endl;
	msg << "	\\info |<username>|" << endl;
	msg << "	\\ranking" << endl;
	msg << "	\\exit" << endl;
	msg << "	\\shutdown";
	
	writeline(sockfd, msg.str());
}

void registerCmd(int sockfd, string username, string password) {
	
	PGresult * result;
	ostringstream sql;
	
	if(username.compare(0,5,"visitor_") == 0) {
		writeline(sockfd, "Prefix 'visitor_' not permitted.");
		return;
	}
		
	sql << "SELECT user_id FROM project.user WHERE username= '" << username << "'";
	result = executeSQL(sql.str());
		
	if(result) {
	
		if(PQntuples(result) == 0) {
			
			sql.str("");
			sql << "INSERT INTO project.user VALUES (default, '" << username << "', '" << password << "', " << START_BALANCE << ", false) RETURNING user_id";
			result = executeSQL(sql.str());
			
			if(result)  {
				
				int user_id = atoi(PQgetvalue(result, 0, 0));
				
				clients[sockfd]->rule = USER;
				clients[sockfd]->username = username;
				clients[sockfd]->id = user_id;
				writeline(sockfd, "> Success. Welcome " + username + "!");
			}
			else
				writeline(sockfd, "> Data base error (2). Check arguments.");
		}
		else
			writeline(sockfd, "> This username already exists! Choose another one.");
	}
	else
		writeline(sockfd, "> Data base error (1). Check arguments.");
}

void identityCmd(int sockfd, string username) {
	
	ostringstream msg;
	
	if(username == "") {
		
		bool processing = true;
			
		while(processing) {
			
			ostringstream random_name;
			
			random_name << "visitor_";
			for(int i = 0; i < 6; i++)
				random_name << rand() % 10;
			
			if(getSockfdByUsername(random_name.str()) < 0) {
				clients[sockfd]->username = random_name.str();
				clients[sockfd]->rule = VISITOR;
				
				msg.str("");
				msg << "> Success. Welcome " << random_name.str() << "!";
				writeline(sockfd, msg.str());
				
				processing = false;
			}
		}
	}
	else {
		
		username = "visitor_" + username;
		
		if(getSockfdByUsername(username)) {
			clients[sockfd]->username = username;
			
			msg.str("");
			msg << "> Success. Welcome " << username << "!";
			writeline(sockfd, msg.str());
		}
		else
			writeline(sockfd, "> This username already exists! Choose another one.");
	}
}

void loginCmd(int sockfd, string username, string password) {
	
	PGresult * result;
	ostringstream msg, sql;
	
	sql << "SELECT user_id, admin FROM project.user WHERE username='" << username << "' AND password='" << password << "'";
	result = executeSQL(sql.str());
	
	if(result) {
		
		if(PQntuples(result) == 1) {

			clients[sockfd]->username = username;
			clients[sockfd]->id = atoi(PQgetvalue(result, 0, 0));
			
			if(strncmp(PQgetvalue(result, 0, 1), "t", 1) == 0)
				clients[sockfd]->rule = ADMIN;
			else
				clients[sockfd]->rule = USER;
			
			msg.str("");
			msg << "> Success. Welcome " << username << "!";
			writeline(sockfd, msg.str());
		}
		else
			writeline(sockfd, "> Fail. Check username and password.");
	}
	else
		writeline(sockfd, "> Data base error. Check arguments.");
}

void logoutCmd(int sockfd) {
	
	delete clients[sockfd];
	clients[sockfd] = new User;
	clients[sockfd]->rule = ANONYMOUS;
	
	writeline(sockfd, "> Success.");
}

void addCmd(int sockfd, string horse, int speed) {
	
	PGresult * result;
	ostringstream sql;
	
	sql << "SELECT * FROM project.horse WHERE name='" << horse << "'";
	result = executeSQL(sql.str());
	
	if(result) {	
	
		if(PQntuples(result) == 0) {
			
			sql.str("");
			sql << "INSERT INTO project.horse VALUES (default, '" << horse << "', " << speed << ")";
			result = executeSQL(sql.str());
			
			if(result)
				writeline(sockfd, "> Success.");
			else
				writeline(sockfd, "> Data base error (2). Check arguments.");
		}
		else
			writeline(sockfd, "> This horse already exists! Choose another one.");
	}
	else
		writeline(sockfd, "> Data base error (1). Check arguments.");
}

void createCmd(int sockfd, string description, int number, int laps) {
	
	PGresult * result;
	ostringstream msg, sql;
	
	//No set os valores são sempre diferentes
	map<string, string> selected_horses;
	map<string, string>::iterator it;
	
	if(number < 3) {
		writeline(sockfd, "> The number of horses must be greater than 1.");
		return;
	}
	
	sql << "SELECT horse_id, name FROM project.horse ORDER BY random()";
	result = executeSQL(sql.str());
	
	if(result) {
		
		if(PQntuples(result) >= number) {
			
			for(int i = 0; i < number; i++) {
				
				string horse_id = PQgetvalue(result, i, 0);
				string horse_name = PQgetvalue(result, i, 1);
				selected_horses[horse_id] = horse_name;
			}
			
			sql.str("");
			sql << "INSERT INTO project.race VALUES (default, '" << description << "', " << laps << ", " << number << ", 'betting');";
			
			for(it = selected_horses.begin(); it != selected_horses.end(); it++)
				sql << "INSERT INTO project.run_in VALUES (" << it->first << ", currval(pg_get_serial_sequence('project.race','race_id')), NULL);";
			
			result = executeSQL(sql.str());		
			
			if(result) {
				
				msg.str("");
				msg << "> Success. Selected horses: ";
				it = selected_horses.begin();
				while(it != selected_horses.end()) {
					msg << it->second;
					msg << ((++it == selected_horses.end())? "." : ", ");
				}
				
				writeline(sockfd, msg.str());
			}				
			else
				writeline(sockfd, "> Data base error (2). Check arguments.");
		}
		else {
			msg.str("");
			msg << "> Fail. The number of existing horses is " << PQntuples(result) << ".";
			writeline(sockfd, msg.str());
		}
	}
	else
		writeline(sockfd, "> Data base error (1). Check arguments.");
}

//TODO
void betCmd(int sockfd, int race_id, string horse, int value) {
	
	PGresult * result;
	ostringstream msg, sql;
	
	sql << "SELECT balance FROM project.user WHERE user_id = " << clients[sockfd]->id;
	sql << " AND user_id NOT IN (SELECT user_id FROM project.bet WHERE race_id = " << race_id << ")";
	result = executeSQL(sql.str());
	
	if(result) {
		
		if(PQntuples(result) == 1) {
			
			int balance = atoi(PQgetvalue(result, 0, 0));
		
			if(value <= balance) {
			
				sql.str("");
				sql << "SELECT horse_id, race_id, state FROM project.run_in ";
				sql << "JOIN project.horse USING (horse_id) ";
				sql << "JOIN project.race USING (race_id) ";
				sql << "WHERE race_id = " << race_id << " AND horse.name = '" << horse << "'";
				result = executeSQL(sql.str());
				
				if(result) {
					
					if(PQntuples(result) == 1) {
						
						char * horse_id = PQgetvalue(result, 0, 0);
						char * race_id = PQgetvalue(result, 0, 1);
						char * state = PQgetvalue(result, 0, 2);
						
						balance -= value;
						
						if(strncmp(state, "betting", 7) == 0) {
							
							sql.str("");
							sql << "UPDATE project.user SET balance = " << balance << " WHERE user_id = " << clients[sockfd]->id << ";";
							sql << "INSERT INTO project.bet VALUES (default, " << value << ", NULL, " << clients[sockfd]->id << ", " << horse_id << ", " << race_id << ")";
							result = executeSQL(sql.str());
							
							if(result) {
								msg.str("");
								msg << "> Success. You bet " << value << " credits on the horse '" << horse << "' for the race " << race_id << ". Your balance is " << balance << " credits.";
								writeline(sockfd, msg.str());
							}			
							else
								writeline(sockfd, "> Data base error (3). Check arguments.");
						}
						else {
							msg.str("");
							msg << "> Fail. The race " << race_id << " is not in betting phase.";
							writeline(sockfd, msg.str());
						}
					}
					else {
						msg.str("");
						msg << "> Fail. The horse '" << horse << "' is not inscribed in race " << race_id << ".";
						writeline(sockfd, msg.str());
					}
				}
				else
					writeline(sockfd, "> Data base error (2). Check arguments.");
			}
			else {
				
				msg.str("");
				msg << "> Fail. Your balance (" << balance << ") is not enough to this bet value (" << value << ").";
				writeline(sockfd, msg.str());
			}	
		}
		else {
			msg.str("");
			msg << "> Fail. You have already bet in race " << race_id << ".";
			writeline(sockfd, msg.str());
		}
	}
	else
		writeline(sockfd, "> Data base error (1). Check arguments.");
}


bool compare_positions (Runner * &runner1, Runner * &runner2) {
	
	return runner1->points >= runner2->points;
}

void sendMsgToPlayers(list<Runner *> runners, string msg) {

	list<Runner *>::iterator runner_it;
	
	for(runner_it = runners.begin(); runner_it != runners.end(); runner_it++) {
		Runner * runner = *runner_it;
		for(int i = 0; i < runner->players.size(); i++) {
			Player * player = runner->players[i];
			int player_socketfd = getSockfdByUserId(player->id);
			if(player_socketfd > 0)
				writeline(player_socketfd, msg);
		}
	}
}

//Inicia a corrida que foi criada primeiro.
void * startCmd(void * args) {
	
	PGresult * result1, * result2, * result3, * result4;
	ostringstream msg, sql;
	
	int sockfd = ((int*)args)[0];
	int race_id = ((int*)args)[1];	
	int laps, number_horses, pot;
	string race_description;
	
	//lista de cavalos participantes
	list<Runner *> runners;
	list<Runner *>::iterator runner_it;

	pthread_mutex_lock(&mutex);
	
	sql << "SELECT state, description, laps, horses FROM project.race WHERE race_id = " << race_id;
	result1 = executeSQL(sql.str());
	
	if(result1) {
		
		if(PQntuples(result1) == 1) {
			
			if(strncmp(PQgetvalue(result1, 0, 0), "betting", 7) == 0) {
				
				race_description = PQgetvalue(result1, 0, 1);
				laps = atoi(PQgetvalue(result1, 0, 2));
				number_horses = atoi(PQgetvalue(result1, 0, 3));
				pot = 0;

				sql.str("");
				sql << "UPDATE project.race SET state = 'running' WHERE race_id = " << race_id << ";";
				sql << "SELECT horse_id, name, speed FROM project.horse JOIN project.run_in USING(horse_id) WHERE race_id = " << race_id;
				result2 = executeSQL(sql.str());
				
				if(result2) {
					
					if(PQntuples(result2) == number_horses) {
						
						srand(time(NULL));
						
						for(int i = 0; i < number_horses; i++) {
						
							Runner * runner = new Runner;				
							runner->id = atoi(PQgetvalue(result2, i, 0));
							runner->name = PQgetvalue(result2, i, 1);
							runner->speed = atoi(PQgetvalue(result2, i, 2));
							runner->performance = (rand() % 100 + 1) * 0.8 + 50 * 0.2;
							runner->total_bet = 0;
							runner->points = 0;
							
							sql.str("");
							sql << "SELECT user_id, value, balance FROM project.bet JOIN project.user USING (user_id) ";
							sql << "WHERE race_id = " << race_id << " AND horse_id = " << runner->id;
							result3 = executeSQL(sql.str());
							
							if(result3) {
								
								for(int j = 0; j < PQntuples(result3); j++) {
									
									Player * player = new Player;
									player->id = atoi(PQgetvalue(result3, j, 0));
									player->bet = atoi(PQgetvalue(result3, j, 1));
									player->balance = atoi(PQgetvalue(result3, j, 2));
									
									runner->players.push_back(player);
									runner->total_bet += player->bet;
									pot += player->bet;
								}			
								runners.push_back(runner);
							}
							else {						
								msg.str("");
								msg << "> Data base error (3) on reading '" << runner->name << "' horse bet values.";
								writeline(sockfd, msg.str());						
								
								for(runner_it = runners.begin(); runner_it != runners.end(); runner_it++)
									delete *runner_it;
								
								pthread_mutex_unlock(&mutex);
								
								return NULL;
							}
						}
			
						//avisa o inicio da corrida
						msg.str("");
						msg << "> Race " << race_id << " (" << race_description << ") started.";
						sendMsgToPlayers(runners, msg.str());
													
						//calcula pontos para cada volta
						for(int lap_num = 1; lap_num <= laps; lap_num++) {
											
							for(runner_it = runners.begin(); runner_it != runners.end(); runner_it++) {
								Runner * runner = *runner_it;
								int lap_performance = rand() % 5 + 1;
								runner->points += lap_performance * runner->speed * runner->performance;
							}							
							
							runners.sort(compare_positions); //ordena os cavalos na lista pelos pontos
							
							pthread_mutex_unlock(&mutex);
							sleep(2);
							pthread_mutex_lock(&mutex);
							
							msg.str("");
							msg << endl << "> Race " << race_id << " - lap " << lap_num << ":" << endl;
							
							int place = 1;
							for(runner_it = runners.begin(); runner_it != runners.end(); runner_it++) {
								Runner * runner = *runner_it;
								runner->place = place;
								msg << "	" << runner->place << " - " << runner->name << " (" << runner->points << " points)" << endl;
								place++;
							}
							
							//mostra resultado da volta
							sendMsgToPlayers(runners, msg.str());
						}
						
						//avisa o final da corrida
						msg.str("");
						msg << "> Race " << race_id << " (" << race_description << ") is over.";
						sendMsgToPlayers(runners, msg.str());
						
						sql.str("");
						sql << "UPDATE project.race SET state = 'finished' " << " WHERE race_id = " << race_id << ";";
						
						//mostra resultado final
						for(runner_it = runners.begin(); runner_it != runners.end(); runner_it++) {
							
							Runner * runner = *runner_it;
							
							sql << "UPDATE project.run_in SET final_position = " << runner->place << " WHERE horse_id = " << runner->id << " AND race_id = " << race_id << ";";
							
							for(int i = 0; i < runner->players.size(); i++) {
								
								Player * player = runner->players[i];
								
								int user_socketfd = getSockfdByUserId(player->id);
								int earned_value = 0;
								float portion = (float)player->bet / (float)runner->total_bet;						
								
								if(runner->place == 1)
									earned_value = pot * 0.6 * portion;
								else if(runner->place == 2)
									earned_value = pot * 0.3 * portion;
								else if(runner->place == 3)
									earned_value = pot * 0.1 * portion;
								
								player->balance += earned_value;
								
								sql << "UPDATE project.bet SET result = " << earned_value << " WHERE race_id = " << race_id << " AND user_id = " << player->id << " AND horse_id = " << runner->id << ";";
								sql << "UPDATE project.user SET balance = " << player->balance << " WHERE user_id = " << player->id << ";";			
								
								if(user_socketfd > 0) {
									msg.str("");
									msg << "You bet " << player->bet << " credits and you won " << earned_value << ". Your balance is " << player->balance << " credits." << endl;
									writeline(user_socketfd, msg.str());
								}

								delete player;
							}

							delete runner;
						}

						result4 = executeSQL(sql.str());
						
						if(result4) {
							msg.str("");
							msg << "> Success. Race " << race_id << " (" << race_description << ") has finished.";
							writeline(sockfd, msg.str());
						}
						else {
							msg.str("");
							msg << "> Data base error (4) on updating race " << race_id << " results.";
							writeline(sockfd, msg.str());
						}
					}
					else {
						msg.str("");
						msg << "> Fail. The number of horses enrolled in race " << race_id << " is not correct.";
						writeline(sockfd, msg.str());
					}
				}
				else
					writeline(sockfd, "> Data base error (2). Check arguments.");
			}
			else {
				msg.str("");
				msg << "> Fail. The race " << race_id << " is not in betting phase.";
				writeline(sockfd, msg.str());
			}	
		}
		else {
			msg.str("");
			msg << "> Fail. The race " << race_id << " does not exist.";
			writeline(sockfd, msg.str());
		}
	}
	else
		writeline(sockfd, "> Data base error (1). Check arguments.");
	
	pthread_mutex_unlock(&mutex);
}

void logCmd(int sockfd, string horse, int limit) {
	
	PGresult * result;
	ostringstream msg, sql;
	
	sql << "SELECT race_id, description, final_position FROM project.run_in ";
	sql << "JOIN project.horse USING(horse_id) JOIN project.race USING(race_id) ";
	sql << "WHERE name = '" << horse << "' AND state = 'finished' ORDER BY race_id DESC LIMIT " << limit;
	result = executeSQL(sql.str());
	
	if(result) {
		
		if(PQntuples(result) > 0) {
			
			msg << "> Horse '" << horse << "' results: " << endl;
			
			for(int row = 0; row < PQntuples(result); row++) {
				
				int race_id = atoi(PQgetvalue(result, row, 0));
				char * description =  PQgetvalue(result, row, 1);
				int place = atoi(PQgetvalue(result, row, 2));
				string place_sufix = "th";
				
				switch(place){
					case 1: place_sufix = "st"; break;
					case 2: place_sufix = "nd"; break;
					case 3: place_sufix = "rd"; break;
				}

				msg << "	race " << race_id << " (" << description << "): " << place << place_sufix << endl;
			}		 

			writeline(sockfd, msg.str());
		}
		else {
			msg.str("");
			msg << "There is no results to horse '" << horse << "'.";
			writeline(sockfd, msg.str());
		}
	}
	else
		writeline(sockfd, "> Data base error (1). Check arguments.");
}

void infoCmd(int sockfd, string username) {
	
	PGresult * result;
	ostringstream msg, sql;
	
	if(username == "")
		username = clients[sockfd]->id;

	sql << "SELECT balance, victories FROM project.user LEFT JOIN (";
	sql << "SELECT user_id, COUNT(*) AS victories FROM project.bet JOIN project.run_in USING(race_id, horse_id) ";
	sql << "WHERE final_position = 1 GROUP BY user_id";
	sql << ") AS temp USING(user_id) WHERE username = '" << username << "'";
	result = executeSQL(sql.str());
		
	if(result) {
		
		if(PQntuples(result) == 1) {
			
			int balance = atoi(PQgetvalue(result, 0, 0));
			int victories = atoi(PQgetvalue(result, 0, 1));

			msg << "> Player '" << username << "' info:" << endl;
			msg << "	balance - " << PQgetvalue(result, 0, 0) << endl;
			msg << "	victories - " << victories;
			
			writeline(sockfd, msg.str());
		}
		else {
			msg.str("");
			msg << "There is no info to user '" << username << "'.";
			writeline(sockfd, msg.str());
		}
	}
	else
		writeline(sockfd, "> Data base error (1). Check arguments.");
}

void agendaCmd(int sockfd) {
	
	PGresult * result;
	ostringstream msg, sql;
	
	sql << "SELECT race_id, state, description, laps, horses, horse.name ";
	sql << "FROM project.run_in ";
	sql << "JOIN project.race USING (race_id) ";
	sql << "JOIN project.horse USING (horse_id) ";
	sql << "WHERE state = 'betting' OR state = 'running'";
	result = executeSQL(sql.str());
	
	if(result) {
		
		if(PQntuples(result) == 0)
			writeline(sockfd, "There is not any scheduled race.");
		else {
			
			int raceId = -1;
			
			msg << "> Agenda:" << endl;
			
			for(int i = 0; i < PQntuples(result); i++) {
				
				if(atoi(PQgetvalue(result, i, 0)) != raceId) {
					
					raceId = atoi(PQgetvalue(result, i, 0));
					
					msg << "	-Race " << raceId << ":" << endl;
					msg << "		state: " << PQgetvalue(result, i, 1) << endl;
					msg << "		description: " << PQgetvalue(result, i, 2) << endl;
					msg << "		number of laps: " << PQgetvalue(result, i, 3) << endl;
					msg << "		horses (" << PQgetvalue(result, i, 4) << "):" << endl;
				}
				
				msg << "			" << PQgetvalue(result, i, 5) << endl;
			}
			
			writeline(sockfd, msg.str());
		}
	}
	else
		writeline(sockfd, "> Data base error (1). Check arguments.");
}

void rakingCmd(int sockfd) {
	
	PGresult * result;
	ostringstream msg, sql;
	
	sql << "SELECT username, balance FROM project.user ORDER BY balance DESC";
	result = executeSQL(sql.str());
	
	if(result) {
		
		msg << "> Raking:" << endl;
		for(int row = 0; row < PQntuples(result); row++)
			msg << "	" << row + 1 << " - " << PQgetvalue(result, row, 0) << " (" << PQgetvalue(result, row, 1) << " credits)" << endl;		
		
		writeline(sockfd, msg.str());
	}
	else
		writeline(sockfd, "> Data base error (1). Check arguments.");
}

void exitCmd(int sockfd) {
	
	delete clients[sockfd];
	clients.erase(sockfd);
	
	writeline(sockfd, "> Success. Bye!");
	
	close(sockfd);
	
	cout << "Client disconnected: " << sockfd << endl;
}

void shutdownCmd(int sockfd) {

	map<int, User *>::iterator it;
	for(it = clients.begin(); it != clients.end(); it++)
		close(it->first);
	
	pthread_mutex_destroy(&mutex);
			
	close(serve_sockfd);
	
	closeDB();
		
	exit(0);
}

/* Trata de receber dados de um cliente cujo socketid foi
   passado como parâmetro */
void* client(void* args) {
	
  int sockfd = *(int*)args;
  string line;
  bool stop_client = false;
  
  User * new_user = new User;
  new_user->rule = ANONYMOUS;
  
  pthread_mutex_lock(&mutex);
  clients[sockfd] = new_user;
  pthread_mutex_unlock(&mutex);

  cout << "Client connected: " << sockfd << endl;
  
  writeline(sockfd, "\n");
  
  while (!stop_client && readline(sockfd, line)) {
	
	if(line.size() > 0) {
		
		vector<string> params;
		string command;	
		  
		cout << "Socket " << sockfd << " said: " << line << endl;
		
		params = split(line, ' ');
		command = params[0];
		
		pthread_mutex_lock(&mutex);
		
		if(command == "\\help") {
			
			if(params.size() != 1)
				writeline(sockfd, "> Usage: \\help");
			else
				helpCmd(sockfd);
		}
		else if(command == "\\register") {

			if(clients[sockfd]->rule > VISITOR)
				writeline(sockfd, "> You are registered already.");
			else if(params.size() != 3)
				writeline(sockfd, "> Usage: \\register <username> <password>");	
			else
				registerCmd(sockfd, params[1], params[2]);
		}
		else if(command == "\\identify") {
			
			if(clients[sockfd]->rule > ANONYMOUS)
				writeline(sockfd, "> You are identified already.");
			else if(params.size() == 1)
				identityCmd(sockfd, "");
			else if(params.size() == 2)
				identityCmd(sockfd, params[1]);
			else
				writeline(sockfd, "> Usage: \\identify |<username>|");
		}
		else if(command == "\\login") {
			
			if(clients[sockfd]->rule > VISITOR)
				writeline(sockfd, "> You are logged in already.");
			else if(params.size() != 3)
				writeline(sockfd, "> Usage: \\login <username> <password>");
			else
				loginCmd(sockfd, params[1], params[2]);
		}
		else if(command == "\\logout") {
			
			if(clients[sockfd]->rule < USER)
				writeline(sockfd, "> You need to login first.");
			else if(params.size() != 1)
				writeline(sockfd, "> Usage: \\logout");
			else
				logoutCmd(sockfd);
		}
		else if(command == "\\add") {
			
			if(clients[sockfd]->rule < ADMIN)
				writeline(sockfd, "> Reserved command.");
			else if(params.size() != 3)
				writeline(sockfd, "> Usage: \\add <horse> <speed>");
			else
				addCmd(sockfd, params[1], atoi(params[2].c_str()));
		}
		else if(command == "\\create") {
			
			if(clients[sockfd]->rule < ADMIN)
				writeline(sockfd, "> Reserved command.");
			else if(params.size() != 4)
				writeline(sockfd, "> Usage: \\create <description> <number> <laps>");
			else
				createCmd(sockfd, params[1], atoi(params[2].c_str()), atoi(params[3].c_str()));
		}
		else if(command == "\\bet") {
			
			if(clients[sockfd]->rule < USER)
				writeline(sockfd, "> You need to login first.");
			else if(params.size() != 4)
				writeline(sockfd, "> Usage: \\bet <raceId> <horse> <value>");
			else
				betCmd(sockfd, atoi(params[1].c_str()), params[2], atoi(params[3].c_str()));
		}
		else if(command == "\\start") {
			
			if(clients[sockfd]->rule < ADMIN)
				writeline(sockfd, "> Reserved command.");
			else if(params.size() != 2)
				writeline(sockfd, "> Usage: \\start <raceId>");
			else {	
				int args[2] = {sockfd, atoi(params[1].c_str())};	
				pthread_t thread;
				pthread_create(&thread, NULL, startCmd, &args);
			}	
		}
		else if(command == "\\log") {
			
			if(clients[sockfd]->rule < VISITOR)
				writeline(sockfd, "> You need to identify yourself first.");
			else if(params.size() != 3)
				writeline(sockfd, "> Usage: \\log <horse> <limit>");
			else
				logCmd(sockfd, params[1], atoi(params[2].c_str()));
		}
		else if(command == "\\info") {
			
			if(clients[sockfd]->rule < USER)
				writeline(sockfd, "> You need to login first.");
			else if(params.size() == 1)
				infoCmd(sockfd, clients[sockfd]->username);
			else if(params.size() == 2)
				infoCmd(sockfd, params[1]);
			else
				writeline(sockfd, "> Usage: \\info |<username>|");
		}
		else if(command == "\\agenda") {
			
			if(clients[sockfd]->rule < VISITOR)
				writeline(sockfd, "> You need to identify yourself first.");
			else if(params.size() != 1)
				writeline(sockfd, "> Usage: \\agenda");
			else
				agendaCmd(sockfd);
		}
		else if(command == "\\ranking") {
			
			if(clients[sockfd]->rule < VISITOR)
				writeline(sockfd, "> You need to identify yourself first.");
			else if(params.size() != 1)
				writeline(sockfd, "> Usage: \\ranking");
			else
				rakingCmd(sockfd);
		}
		else if(command == "\\exit") {
			
			if(params.size() != 1)
				writeline(sockfd, "> Usage: \\exit");
			else {
				exitCmd(sockfd);
				stop_client = true;
			}	
		}	
		else if(command == "\\shutdown") {
			
			if(clients[sockfd]->rule < ADMIN)
				writeline(sockfd, "> Reserved command.");
			else if(params.size() != 1)
				writeline(sockfd, "> Usage: \\shutdown");
			else
				shutdownCmd(sockfd);
		}
		else {
			writeline(sockfd, "> Command wrong.");
		}

		writeline(sockfd, "\n");
		
		pthread_mutex_unlock(&mutex);
	}
  }
  
  writeline(sockfd, "\n");
}

int main(int argc, char *argv[]) {
	
	int newsockfd, port = 3254;
	struct sockaddr_in serv_addr, cli_addr;
	socklen_t client_addr_length = sizeof(cli_addr);
  
	if(argc != 2) {
		cout << "usage: server <port>" << endl;
		exit(-1);
	}

	port = atoi(argv[1]);
  
	serve_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (serve_sockfd < 0) {
		cout << "Error creating socket" << endl;
		exit(-1);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	int res = bind(serve_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (res < 0) {
		cout << "Error binding to socket" << endl;
		exit(-1);
	}

	listen(serve_sockfd, 5);

	initDB();
	
	cout << "Server started." << endl;
	
	pthread_mutex_init(&mutex, NULL);
  
	while(true) {
			
		if(newsockfd = accept(serve_sockfd, (struct sockaddr *) &cli_addr, &client_addr_length)) {
		
			pthread_t thread;
			pthread_create(&thread, NULL, client, &newsockfd);
		}
		else
			cout << "error: " << newsockfd << endl;
	}
  
	return 0; 
}
