const express = require('express');
const router = express.Router();
const db = require('../db');

router.post('/', (req, res) => {
    const {stockID} = req.body;
    req.session.stockID = stockID;
    var orderPrice = 0;
    db.query(`
        SELECT order_price
        FROM buy
        WHERE stock_id = ?
        ORDER BY order_price DESC, order_id ASC
    `, [stockID], (error, results, field) => {
        if(error) throw error;
        if(results.length === 0){
            res.render('sellPage', {marketPrice : orderPrice});
            return;
        }
        orderPrice = results[0].order_price;
        res.render('sellPage', {marketPrice : orderPrice});
    });
});

// 매도자 보유 주식 감소시키기
const update_seller_holding_stock = async (user_id, stock_id, order_volume) => {
    try {
        const [results] = await db.promise().query(`
            SELECT holding_num
            FROM holding_stock
            WHERE user_id = ? AND stock_id = ?
        `, [user_id, stock_id]);

        if(results.length === 0){
            throw error;
        }
        const holding_num = results[0].holding_num;

        if (holding_num > order_volume) {
            // 보유 주식이 거래량보다 많을 때 보유 주식 수와 거래 가능 주식 수 감소시키기
            await db.promise().query(`
                UPDATE holding_stock
                SET holding_num = holding_num - ?, transaction_available = transaction_available - ?
                WHERE user_id = ? AND stock_id = ?
            `, [order_volume, order_volume, user_id, stock_id]);
        } else if (holding_num === order_volume) {
            // 보유 주식이 거래량과 같을 때 보유 주식 삭제
            await db.promise().query(`
                DELETE FROM holding_stock
                WHERE user_id = ? AND stock_id = ?
            `, [user_id, stock_id]);
        }
    } catch (error) {
        throw error; 
    }
};

const update_buyer_holding_stock = async (user_id, stock_id, price, order_volume) => {
    try{
        const [results] = await db.promise().query(`
            SELECT holding_num, buying_price
            FROM holding_stock
            WHERE user_id = ? and stock_id = ?
        `, [user_id, stock_id]);

        //해당 주식 보유수량이 0개일때
        if(results.length === 0){
            //새로운 tuple 추가해줌
            await db.promise().query(`
                INSERT INTO holding_stock(user_id, stock_id, buying_price, transaction_available, holding_num)
                VALUES (?,?,?,?,?)
            `, [user_id, stock_id, price, order_volume, order_volume]);
        }
        //해당 주식을 보유하고 있을 때
        else{
            //값을 갱신해줌. 중요한 것은 buying_price를 비율에 맞게 해준다는 것
            const new_buying_price = parseFloat(
                (
                    (parseInt(results[0].buying_price) * parseInt(results[0].holding_num) + price * order_volume) /
                    (parseInt(results[0].holding_num) + parseInt(order_volume))
                ).toFixed(2)
            );
            await db.promise().query(`
                UPDATE holding_stock
                SET holding_num = holding_num + ?, transaction_available = transaction_available + ?, buying_price = ?
                WHERE user_id = ? and stock_id = ?
            `, [order_volume, order_volume, new_buying_price, user_id, stock_id]);
        }
    }catch (error){
        throw error;
    }
};

const stockOrder = async (stockID, price, Amount, isMarketPrice, req) => {
    try {
        const [marketPrice] = await db.promise().query(`
            SELECT order_price
            FROM buy
            WHERE stock_id = ?
            ORDER BY order_price DESC, order_id ASC
        `, [stockID]);

        // 남아있는 주문이 없으면 빠르게 주문만 넣고 종료
        if (marketPrice.length === 0) {
            await db.promise().query(`
                INSERT INTO sell(user_id, stock_id, order_volume, order_type, order_price, order_time)
                VALUES (?,?,?,?,?,?)
            `, [req.session.userID, req.session.stockID, Amount, 1, price, new Date()]);

            //매도자 주식 거래 가능 수량 업데이트
            await db.promise().query(`
                UPDATE holding_stock
                SET transaction_available = transaction_available - ?
                WHERE user_id = ? and stock_id = ?
            `, [Amount, req.session.userID, req.session.stockID]);
            return;
        }

        // 지정가 매도 주문을 걸었는데 그 지정가가 시장가보다 낮으면 시장가 거래로 처리
        if (price <= marketPrice[0].order_price) {
            isMarketPrice = 'true';
            price = marketPrice[0].order_price;
        }

        // 시장가 주문
        if (isMarketPrice === 'true') {
            while (Amount > 0) {
                // 가장 비싼 오래된 매수 주문 선택
                var [buyOrder] = await db.promise().query(`
                    SELECT *
                    FROM buy
                    WHERE stock_id = ? and order_price = ?
                    ORDER BY order_id ASC
                    LIMIT 1
                `, [stockID, price]);

                // 남아있는 시장가 최대 금액 매수 주문이 없을 때
                if (buyOrder.length === 0) {
                    [buyOrder] = await db.promise().query(`
                        SELECT *
                        FROM buy
                        WHERE stock_id = ?
                        ORDER BY order_price DESC, order_id ASC
                        LIMIT 1
                    `, [stockID]);

                    // 진짜 남아있는 매도 주문이 없을 때
                    if (buyOrder.length === 0) {
                        await db.promise().query(`
                            INSERT INTO sell(user_id, stock_id, order_volume, order_type, order_price, order_time)
                            VALUES (?,?,?,?,?,?)
                        `, [req.session.userID, req.session.stockID, Amount, 1, price, new Date()]);

                        //매도자 거래 가능 수량 업데이트
                        await db.promise().query(`
                            UPDATE holding_stock
                            SET transaction_available = transaction_available - ?
                            WHERE user_id = ? and stock_id = ?
                        `, [Amount, req.session.userID, req.session.stockID]);
                        return;
                    }
                }

                const { order_id, user_id, stock_id, order_volume, order_price } = buyOrder[0];
                price = order_price;

                // 매수 주문 수량이 매도 주문 수량보다 적을 때
                if (Amount > order_volume) {
                    Amount -= order_volume;

                    // 매수주문 제거
                    await db.promise().query(`
                        DELETE FROM buy 
                        WHERE order_id = ?
                    `, [order_id]);

                    // 지운 매수 주문을 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [user_id, stock_id, -order_price, order_volume, new Date()]);

                    // //매수자 예수금 감소시키기
                    // await db.promise().query(`
                    //     UPDATE user
                    //     SET user_deposit = user_deposit - ?
                    //     WHERE user_id = ?
                    // `, [order_price * order_volume, user_id]);

                    //매수자 보유주식 업데이트
                    await update_buyer_holding_stock(user_id, stock_id, order_price, order_volume);

                    // 체결된 매도주문을 주식 거래내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, order_price, order_volume, new Date()]);

                    // 매도자 예수금 증가시키기
                    await db.promise().query(`
                        UPDATE user
                        SET user_deposit = user_deposit + ?
                        WHERE user_id = ?
                    `, [order_price * order_volume, req.session.userID]);

                    //매도자 보유주식 업데이트
                    await update_seller_holding_stock(req.session.userID, req.session.stockID, order_volume);

                    //현재가 업데이트
                    await db.promise().query(`
                        UPDATE stock
                        SET current_price = ?
                        WHERE stock_id = ?
                    `, [price, stock_id]);

                    //거래가 체결되었으므로 거래량 업데이트
                    await db.promise().query(`
                        UPDATE stock
                        SET trading_volume = trading_volume + ?
                        WHERE stock_id = ?
                    `, [order_volume, stock_id]);
                }
                // 매도 주문 수량이 매수 주문 수량보다 많은 경우
                else if (Amount < order_volume) {
                    // 매수 주문에서 수량을 제거
                    await db.promise().query(`
                        UPDATE buy
                        SET order_volume = order_volume - ?
                        WHERE order_id = ?
                    `, [Amount, order_id]);

                    // 매수 주문 체결된 것 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [user_id, stock_id, -order_price, Amount, new Date()]);

                    // //매수자 예수금 감소시키기
                    // await db.promise().query(`
                    //     UPDATE user
                    //     SET user_deposit = user_deposit - ?
                    //     WHERE user_id = ?
                    // `, [Amount * order_price, user_id]);

                    //매수자 보유주식 업데이트
                    await update_buyer_holding_stock(user_id, stock_id, order_price, Amount);

                    //매도자 거래내역 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, price, Amount, new Date()]);

                    //매도자 예수금 증가
                    await db.promise().query(`
                        UPDATE user
                        SET user_deposit = user_deposit + ?
                        WHERE user_id = ?
                    `, [Amount * order_price, req.session.userID]);

                    //매도자 보유주식 업데이트
                    await update_seller_holding_stock(req.session.userID, req.session.stockID, Amount);

                    //현재가 업데이트
                    await db.promise().query(`
                        UPDATE stock
                        SET current_price = ?
                        WHERE stock_id = ?
                    `, [price, stock_id]);

                    //거래가 체결되었으므로 거래량 업데이트
                    await db.promise().query(`
                        UPDATE stock
                        SET trading_volume = trading_volume + ?
                        WHERE stock_id = ?
                    `, [Amount, stock_id]);
                    Amount = 0;
                }
                // 수량이 동일할 때
                else {
                    // 매수 주문에서 해당 주문 제거
                    await db.promise().query(`
                        DELETE FROM buy
                        WHERE order_id = ?
                    `, [order_id]);

                    // 매수 주문을 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?) 
                    `, [user_id, stock_id, -price, order_volume, new Date()]);

                    // //매수자 예수금 감소
                    // await db.promise().query(`
                    //     UPDATE user
                    //     SET user_deposit = user_deposit - ?
                    //     WHERE user_id = ?
                    // `, [order_volume * order_price, user_id]);

                    //매수자 보유주식 업데이트
                    await update_buyer_holding_stock(user_id, stock_id, order_price, order_volume);

                    // 매도 주문을 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?) 
                    `, [req.session.userID, req.session.stockID, price, Amount, new Date()]);

                    //매도자 예수금 증가
                    await db.promise().query(`
                        UPDATE user
                        SET user_deposit = user_deposit + ?
                        WHERE user_id = ?
                    `, [Amount * price, req.session.userID]);

                    //매도자 보유주식 업데이트
                    await update_seller_holding_stock(req.session.userID, req.session.stockID, Amount);

                    //현재가 업데이트
                    await db.promise().query(`
                        UPDATE stock
                        SET current_price = ?
                        WHERE stock_id = ?
                    `, [price, stock_id]);

                    //거래가 체결되었으므로 거래량 업데이트
                    await db.promise().query(`
                        UPDATE stock
                        SET trading_volume = trading_volume + ?
                        WHERE stock_id = ?
                    `, [Amount, stock_id]);
                    Amount = 0;
                }
            }
        }
        // 지정가 주문인 경우
        else {
            while (Amount > 0) {
                // 해당 지정가에 걸려있는 매수 주문을 오래된 것부터 천천히 처리
                const [sellOrder] = await db.promise().query(`
                    SELECT *
                    FROM buy
                    WHERE stock_id = ? and order_price = ?
                    ORDER BY order_id ASC
                    LIMIT 1
                `, [stockID, price]);

                // 해당 지정가에 걸려있는 매수 주문이 없으면
                if (sellOrder.length === 0) {
                    await db.promise().query(`
                        INSERT INTO sell(user_id, stock_id, order_volume, order_type, order_price, order_time)
                        VALUES (?,?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, Amount, 1, price, new Date()]);
                    return;
                }

                const { order_id, stock_id, user_id, order_volume, order_price } = sellOrder[0];

                // 매수 주문 수량이 매도 주문 수량보다 적을 때
                if (Amount > order_volume) {
                    Amount -= order_volume;

                    // 매수주문이 처리되었으니 매도 주문 제거
                    await db.promise().query(`
                        DELETE FROM buy
                        WHERE order_id = ?
                    `, [order_id]);

                    // 매수 주문을 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [user_id, stock_id, -order_price, order_volume, new Date()]);

                    // 매도 주문 남은 잔량 매도 주문에 추가
                    await db.promise().query(`
                        INSERT INTO sell(user_id, stock_id, order_volume, order_type, order_price, order_time)
                        VALUES (?,?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, Amount, 1, price, new Date()]);

                    // 매도 주문 체결된 것은 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, price, order_volume, new Date()]);
                }
                // 매수 주문 잔량이 더 많은 경우
                else if (Amount < order_volume) {
                    // 매수 주문 잔량 업데이트
                    await db.promise().query(`
                        UPDATE buy
                        SET order_volume = order_volume - ?
                        WHERE order_id = ?
                    `, [Amount, order_id]);

                    // 매수 주문 처리된 것은 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [user_id, stock_id, -price, Amount, new Date()]);

                    // 체결된 매도 주문 주식 거래내역으로 이동
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, price, Amount, new Date()]);
                    Amount = 0;
                }
                // 매도 주문과 매수 주문의 수량이 일치하는 경우
                else {
                    // 매수 주문에서 해당 주문 지우기
                    await db.promise().query(`
                        DELETE FROM buy
                        WHERE order_id = ?
                    `, [order_id]);

                    // 매수 주문을 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [user_id, stock_id, -price, Amount, new Date()]);

                    // 매도 주문을 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, price, Amount, new Date()]);
                    Amount = 0;
                }
            }
        }
    } catch (error) {
        throw error;
    }
};


router.post('/confirm', async (req, res) => {
    try{
        const {Price, Amount, isMarketPrice} = req.body;
        const [results] = await db.promise().query(`
            SELECT *
            FROM holding_stock
            WHERE user_id = ? and stock_id = ?
        `, [req.session.userID, req.session.stockID]);

        if(results.length === 0){
            res.render('menuPage', {sellOrder : 'fail'});
            return;
        }

        if(results[0].transaction_available < Amount){
            res.render('menuPage', {sellOrder : 'fail'});
            return;
        }
        await stockOrder(req.session.stockID, Price, Amount, isMarketPrice, req);
        res.render('menuPage', {sellOrder : 'success'});
    }catch (error){
        throw error;
    }
});

module.exports = router;