CREATE TABLE project.user
(
	user_id SERIAL PRIMARY KEY,
	username varchar(20) NOT NULL UNIQUE,
	password varchar(20) NOT NULL,
	balance numeric NOT NULL,
	admin boolean NOT NULL
);

CREATE TABLE project.horse
(
	horse_id SERIAL PRIMARY KEY,
	name varchar (100) NOT NULL UNIQUE,
	speed integer CHECK (speed > 0 AND speed <= 100) NOT NULL
);

CREATE TABLE project.race
(
	race_id SERIAL PRIMARY KEY,
	description varchar (200),
	laps integer CHECK (laps > 0 AND laps <= 50) NOT NULL,
	horses integer CHECK (horses > 0 AND horses <= 20) NOT NULL,
	state varchar(20) CHECK (state = 'betting' OR state = 'running' OR state = 'finished')
);




CREATE TABLE project.bet
(
	bet_id SERIAL PRIMARY KEY,
	value integer CHECK (value > 0 AND value <= 10) NOT NULL,
	result integer,
	user_id integer REFERENCES project.user NOT NULL,
	horse_id integer REFERENCES project.horse NOT NULL,
	race_id integer REFERENCES project.race NOT NULL,
	CONSTRAINT unique_bet UNIQUE (user_id, horse_id, race_id)
);



CREATE TABLE project.run_in
(
	horse_id integer REFERENCES project.horse,
	race_id integer REFERENCES project.race,
	final_position integer CHECK (final_position > 0),
	PRIMARY KEY (horse_id, race_id)
);



INSERT INTO project.user VALUES ('sinfprojetct', 'sinf16g23', 1500, 1);
INSERT INTO project.user VALUES ('lovehorses16', 'sinf16', 1500, 0);

INSERT INTO project.horse VALUES (‘Tom Fool’, 70);
INSERT INTO project.horse VALUES ('Native Dancer', 65);
INSERT INTO project.horse VALUES ('Doctor Who', 55);
INSERT INTO project.horse VALUES ('Sky Fall', 10);
INSERT INTO project.horse VALUES ('Carry Back', 80);
INSERT INTO project.horse VALUES ('Purple Prince', 90);
INSERT INTO project.horse VALUES ('Palace Malice', 20);
INSERT INTO project.horse VALUES ('Rachel Alexandra', 15);
INSERT INTO project.horse VALUES ('John Henry', 35);
INSERT INTO project.horse VALUES ('Sunday Silence', 40);

INSERT INTO project.bet VALUES (5, 100);

INSERT INTO project.race VALUES ('Feupracing', 25, 5);
INSERT INTO project.race VALUES ('Sinfrace', 30, 9);
