DROP DATABASE IF EXISTS DB_2022006135;
CREATE DATABASE DB_2022006135;
USE DB_2022006135;

CREATE TABLE user(
    user_id varchar(20) NOT NULL,
    user_name varchar(20) NOT NULL,
    user_password varchar(20) NOT NULL,
    user_age INT NOT NULL,
    user_deposit INT NOT NULL,
    PRIMARY KEY (user_id)
);

CREATE TABLE stock(
    stock_id INT AUTO_INCREMENT NOT NULL,
    stock_name varchar(20) NOT NULL,
    current_price INT,
    closing_price INT,
    price_unit INT,
    trading_volume INT NOT NULL,
    PRIMARY KEY (stock_id)
);

CREATE TABLE holding_stock(
    user_id varchar(20) NOT NULL,
    stock_id INT NOT NULL,
    buying_price INT NOT NULL,
    transaction_available INT NOT NULL,
    holding_num INT NOT NULL,
    PRIMARY KEY (user_id, stock_id),
    FOREIGN KEY (user_id) REFERENCES user(user_id) ON DELETE CASCADE,
    FOREIGN KEY (stock_id) REFERENCES stock(stock_id) ON DELETE CASCADE
);

CREATE TABLE stock_trading_log(
    transaction_id INT AUTO_INCREMENT NOT NULL,
    user_id varchar(20) NOT NULL,
    stock_id INT NOT NULL,
    buy_sell INT NOT NULL,
    transaction_volume INT NOT NULL,
    transaction_time DATETIME NOT NULL,
    PRIMARY KEY (transaction_id),
    FOREIGN KEY (user_id) REFERENCES user(user_id) ON DELETE CASCADE,
    FOREIGN KEY (stock_id) REFERENCES stock(stock_id) ON DELETE CASCADE
);

CREATE TABLE sell(
    order_id INT AUTO_INCREMENT NOT NULL,
    user_id varchar(20) NOT NULL,
    stock_id INT NOT NULL,
    order_volume INT NOT NULL,
    order_type INT NOT NULL,
    order_price INT NOT NULL,
    order_time DATETIME NOT NULL,
    PRIMARY KEY (order_id),
    FOREIGN KEY (user_id) REFERENCES user(user_id) ON DELETE CASCADE,
    FOREIGN KEY (stock_id) REFERENCES stock(stock_id) ON DELETE CASCADE
);
CREATE TABLE buy(
    order_id INT AUTO_INCREMENT NOT NULL,
    user_id varchar(20) NOT NULL,
    stock_id INT NOT NULl,
    order_volume INT NOT NULL,
    order_type INT NOT NULL,
    order_price INT NOT NULL,
    order_time DATETIME NOT NULL,
    PRIMARY KEY (order_id),
    FOREIGN KEY (user_id) REFERENCES user(user_id) ON DELETE CASCADE,
    FOREIGN KEY (stock_id) REFERENCES stock(stock_id) ON DELETE CASCADE
);


