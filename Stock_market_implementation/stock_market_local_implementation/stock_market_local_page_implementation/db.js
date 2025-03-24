const mysql = require('mysql2');

const db_info = {
    host:   'localhost',
    port:   '3306',
    user:   'root',
    password:   'audgus00543',
    database:   'DB_2022006135'
};

const sql_connection = mysql.createConnection(db_info);
sql_connection.connect((error) => {
    if (error) throw error;
    else{
        console.log('connected to mysql');
    }
});

module.exports = sql_connection;