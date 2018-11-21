if(!(username.find(sockfd) != username.end())){
		writeline(sockfd,"Erro: Não está ligado, conecte-se primeiro!");
		return ;
	}
	
	string balance;
		
	PGresult* result = executeSQL("SELECT * FROM username");
	for (int row = 0; row < PQntuples(resukt); row++){
		if(PQgetvalue(res, row, 0) == username[sockfd]){
			balance=PQgetvalue(result, row, 2);
			writeline(sockfd,"Os seus créditos são: " +balance);
		}
	}	