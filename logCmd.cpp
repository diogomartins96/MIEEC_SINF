PGresult * result = executeSQL("SELECT * FROM horse");
	for (int row = 0; row < PQntuples(result); row++)
		if(!horse.compare(PQgetvalue(result, row, 0))) {
			break;
		}
		else if (row==(PQntuples(result)-1)) {
			writeline(sockfd,"Erro: Cavalo não existe, tente novamente!");
			return ;
		}
	
	PGresult* result2 = executeSQL("SELECT * FROM corre");
	for (int row = 0; row < PQntuples(result2); row++)
		if(!horse.compare(PQgetvalue(result2, row, 0))){
			break;
		}
		else if (row==(PQntuples(res2)-1)) {
			writeline(sockfd,"O cavalo não participou em nenhuma corrida!");
			return ;
		}
	
	for (int row = 0; row < PQntuples(result2); row++){
		if(!horse.compare(PQgetvalue(result2, row, 0))){
			ref = PQgetvalue(result2, row, 1);
			final_position = PQgetvalue(result2, row, 2);
			writeline(sockfd, "Corrida: " + ref + "   final_position: " + final_position);
		}