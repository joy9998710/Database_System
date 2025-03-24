INSERT INTO user(user_id, user_name, user_password, user_age, user_deposit)
VALUES ('joy999871', '문준영', 'answnsdud55!', 24, 10000000);

INSERT INTO user(user_id, user_name, user_password, user_age, user_deposit)
VALUES ('wnwjdgus', '주정현', 'wnwjdgus', 24, 10000000);

INSERT INTO user(user_id, user_name, user_password, user_age, user_deposit)
VALUES ('rkdtndyd', '강수용', 'rkdtndyd', 22, 10000000);

delete from stock;
DELETE FROM stock_trading_log;
DELETE FROM holding_stock;
DELETE from buy;
DELETE from sell;

INSERT INTO stock(stock_name, current_price, closing_price, price_unit, trading_volume)
VALUES ('수용전자', 57300, 57600, 100, 0);

INSERT INTO stock(stock_name, current_price, closing_price, price_unit, trading_volume)
VALUES ('정현차', 206500, 215000, 500, 0);

INSERT INTO stock(stock_name, current_price, closing_price, price_unit, trading_volume)
VALUES ('OS', 76000, 76700, 100, 0);

INSERT INTO stock(stock_name, current_price, closing_price, price_unit, trading_volume)
VALUES ('DB', 176300, 176500, 100, 0);

INSERT INTO stock(stock_name, current_price, closing_price, price_unit, trading_volume)
VALUES ('언규군사물자', 10000, 10300, 100, 0);

INSERT INTO holding_stock
VALUES ('joy999871', 1, 57400, 100, 100);

INSERT INTO holding_stock
VALUES ('wnwjdgus', 2, 210000, 100, 100);

INSERT INTO holding_stock
VALUES ('rkdtndyd', 3, 76500, 100, 100);

INSERT INTO holding_stock
VALUES ('rkdtndyd', 4, 176200, 100, 100);

INSERT INTO holding_stock
VALUES ('rkdtndyd', 5, 9900, 100, 100);