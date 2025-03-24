const express = require('express');
const router = express.Router();
const db = require('../db');

router.post('/', (req, res) => {
    const {stockID} = req.body;
    req.session.stockID = stockID;
    var orderPrice = 0;
    db.query(`
        SELECT order_price
        FROM sell
        WHERE stock_id = ?
        ORDER BY order_price ASC, order_id ASC
        LIMIT 1
    `,[stockID], (error, results, field) => {
        if (error) throw error;
        if(results.length === 0){
            res.render('buyPage', {marketPrice : orderPrice});
            return;
        }
        orderPrice = results[0].order_price;
        res.render('buyPage', {marketPrice : orderPrice});
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
                SET holding_num = holding_num - ?
                WHERE user_id = ? AND stock_id = ?
            `, [order_volume, user_id, stock_id]);
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
    try{
        const [marketPrice] = await db.promise().query(`
            SELECT order_price
            FROM sell
            WHERE stock_id = ?
            ORDER BY order_price ASC, order_id ASC
        `, [stockID]);

        if(marketPrice.length === 0){
            await db.promise().query(`
                INSERT INTO buy(user_id, stock_id, order_volume, order_type, order_price, order_time)
                VALUES (?,?,?,?,?,?)
            `, [req.session.userID, req.session.stockID, Amount, 0, price, new Date()]);

            // await db.promise().query(`
            //     UPDATE holding_stock
            //     SET transaction_available = transaction_available - ?
            //     WHERE user_id = ? and stock_id = ?
            // `, [Amount, req.session.userID, req.session.stockID]);

            //매수자 예수금 차감
            await db.promise().query(`
                UPDATE user
                SET user_deposit = user_deposit - ?
                WHERE user_id = ?
            `, [Amount * price, req.session.userID]);
            return;
        }

        //지정가 매수 주문을 걸었는데 그 지정가가 시장가보다 높은 경우는 시장가 거래로 함
        if(price >= marketPrice[0].order_price){
            isMarketPrice = 'true';
            price = marketPrice[0].order_price;
        }
        if(isMarketPrice === 'true'){
            //시장가 주문
            while(Amount > 0){
                //먼저 시장가 tuple 꺼내기 여기서 중요한 것은 시장가 주문이기에 그냥 가장 적고 오래된 금액을 찾으면 됨
                var [sellOrder] = await db.promise().query(`
                    SELECT *
                    FROM sell
                    WHERE stock_id = ? and order_price = ?
                    ORDER BY order_id ASC
                    LIMIT 1
                `, [stockID, price]);
                //남아있는 시장가 최소 금액 매도 주문이 없을 때
                if(sellOrder.length === 0){
                    //최소 금액 매도 주문 찾기
                    [sellOrder] = await db.promise().query(`
                        SELECT *
                        FROM sell
                        WHERE stock_id = ?
                        ORDER BY order_price ASC, order_id ASC
                        LIMIT 1
                    `,[stockID]);
                    //진짜 남아있는 매도 주문이 없을 때
                    if(sellOrder.length === 0){
                        //매수 주문 내역에 추가하고 끝
                        await db.promise().query(`
                            INSERT INTO buy(user_id, stock_id, order_volume, order_type, order_price, order_time)
                            VALUES (?,?,?,?,?,?)
                        `, [req.session.userID, stockID, Amount, 0, price, new Date()])

                        // //매수자 거래가능수량 업데이트
                        // await db.promise().query(`
                        //     UPDATE holding_stock
                        //     SET transaction_available = transaction_available - ?
                        //     WHERE user_id = ? and stock_id = ?
                        // `, [Amount, req.session.userID, req.session.stockID]);

                        //매수자 예수금 감소시키기
                        await db.promise().query(`
                            UPDATE user
                            SET user_deposit = user_deposit - ?
                            WHERE user_id = ?
                        `, [price * Amount, req.session.userID]);
                        return;
                    }
                }
                //이제는 sellOrder가 존재할 것임
                const {order_id, user_id, stock_id, order_volume, order_price} = sellOrder[0];
                price = order_price;

                //매도 주문으로 올라온게 현재 매수주문을 걸은 수량보다 많을 때
                if(Amount > order_volume){
                    Amount -= order_volume;

                    //현재 tuple을 매도 주문 relation에서 제거하고 주식 거래 내역 relation으로 보내기
                    await db.promise().query(`
                        DELETE FROM sell
                        WHERE order_id = ?
                    `, [order_id]);
                    
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [user_id, stock_id, order_price, order_volume, new Date()]);

                    //매도자 예수금 증가시키기
                    await db.promise().query(`
                        UPDATE user
                        SET user_deposit = user_deposit + ?
                        WHERE user_id = ?
                    `, [order_volume * order_price, user_id]);

                    //매도자의 보유주식 업데이트
                    await update_seller_holding_stock(user_id, stock_id, order_volume);

                    //매수자 거래내역 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, -order_price, order_volume, new Date()]);

                    //매수자 예수금 감소시키기
                    await db.promise().query(`
                        UPDATE user
                        SET user_deposit = user_deposit - ?
                        WHERE user_id = ?
                    `, [order_volume * order_price, req.session.userID]);

                    //매수자 보유주식 업데이트
                    await update_buyer_holding_stock(req.session.userID, req.session.stockID, price, order_volume);
                    //현재가 업데이트
                    await db.promise().query(`
                        UPDATE stock
                        SET current_price = ?
                        WHERE stock_id = ?
                    `, [price, stock_id]);

                    //거래가 체결된 것이므로 거래량 업데이트
                    await db.promise().queery(`
                        UPDATE stock
                        SET trading_volume = trading_volume + ?
                        WHERE stock_id = ?
                    `, [order_volume, stock_id]);
                }
                //매도 주문 수량이 매수 주문 수량보다 많은 경우
                else if (Amount < order_volume){
                    //매도 주문에서 수량을 제거
                    await db.promise().query(`
                        UPDATE sell
                        SET order_volume = order_volume - ?
                        WHERE order_id = ?
                    `, [Amount, order_id]);

                    //매도 주문 체결된 것 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?) 
                    `, [user_id, stock_id, order_price, Amount, new Date()]);

                    //매도자 주문 체결된 주식 예수금에 증가시키기
                    await db.promise().query(`
                        UPDATE user
                        SET user_deposit = user_deposit + ?
                        WHERE user_id = ?
                    `, [Amount * order_price, user_id]);

                    //매도자 보유주식 업데이트
                    await update_seller_holding_stock(user_id, stock_id, Amount);

                    //매수 주문내역을 주식 거래 내역 relation에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, -price, Amount, new Date()]);

                    //매수자 예수금 감소
                    await db.promise().query(`
                        UPDATE user
                        SET user_deposit = user_deposit - ?
                        WHERE user_id = ?
                    `, [Amount * order_price, req.session.userID]);

                    //매수자 보유주식 업데이트
                    await update_buyer_holding_stock(req.session.userID, req.session.stockID, price, Amount);

                    //현재가 업데이트
                    await db.promise().query(`
                        UPDATE stock
                        SET current_price = ?
                        WHERE stock_id = ?
                    `, [price, stock_id]);

                    //거래가 체결된 것이므로 거래량 업데이트
                    await db.promise().query(`
                        UPDATE stock
                        SET trading_volume = trading_volume + ?
                        WHERE stock_id = ?
                    `,[Amount, stock_id]);
                    Amount = 0;
                }
                else{
                    //매도주문에서 해당 주문 제거
                    await db.promise().query(`
                        DELETE FROM sell
                        WHERE order_id = ?
                    `, [order_id]);

                    //매도 주문을 주식 거래내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?) 
                    `, [user_id, stock_id, price, order_volume, new Date()]);
                    
                    //매도자 예수금 증가시키기
                    await db.promise().query(`
                        UPDATE user
                        SET user_deposit = user_deposit + ?
                        WHERE user_id = ?
                    `, [order_volume * order_price, user_id]);

                    //매도자 보유주식 업데이트
                    await update_seller_holding_stock(user_id, stock_id, order_volume);

                    //매수 주문을 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?) 
                    `, [req.session.userID, req.session.stockID, -price, Amount, new Date()]);

                    //매수자 예수금 감소시키기
                    await db.promise().query(`
                        UPDATE user
                        SET user_deposit = user_deposit - ?
                        WHERE user_id = ?
                    `, [Amount * order_price, req.session.userID]);
                        
                    //매수자 보유 주식 업데이트
                    await update_buyer_holding_stock(req.session.userID, req.session.stockID, price, Amount);

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
        //지정가 주문인 경우
        else{
            while(Amount > 0){
                //해당 지정가에 걸려있는 매도 주문을 오래된 것부터 천천히 처리
                const [sellOrder] = await db.promise().query(`
                    SELECT *
                    FROM sell
                    WHERE stock_id = ? and order_price = ?
                    ORDER BY order_id ASC
                    LIMIT 1
                `, [stockID, price]);
                //만약 해당 지정가에 걸려있는 매도 주문이 없으면
                if(sellOrder.length === 0){
                    await db.promise().query(`
                        INSERT INTO buy(user_id, stock_id, order_volume, order_type, order_price, order_time)
                        VALUES (?,?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, Amount, 0, price, new Date()]);

                    await db.promise().query(`
                        UPDATE user
                        SET user_deposit = user_deposit - ?
                        WHERE user_id = ?
                    `, [Amount * price, req.session.userID]);
                    return;
                }

                const {order_id, stock_id, user_id, order_volume, order_price} = sellOrder[0];
                //매도 주문 수량이 매수 주문 수량보다 적을 때
                if(Amount > order_volume){
                    Amount -= order_volume;

                    //매도 주문이 처리 되었으니 일단 해당 매도 주문 제거
                    await db.promise().query(`
                        DELETE FROM sell
                        WHERE order_id = ?
                    `, [order_id]);

                    //매도 주문을 주식 거래내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [user_id, stock_id, order_price, order_volume, new Date()]);
                    
                    //매수 주문 남은 잔량 매수 주문에 추가
                    await db.promise().query(`
                        INSERT INTO buy(user_id, stock_id, order_volume, order_type, order_price, order_time)
                        VALUES (?,?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, Amount, 0, price, new Date()]);
                    //매수 주문 체결된 것은 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, -price, order_volume, new Date()]);
                }
                //매도 주문 잔량이 더 많은 경우
                else if(Amount < order_volume){
                    //매도 주문 잔량 업데이트
                    await db.promise().query(`
                        UPDATE sell
                        SET order_volume = order_volume - ?
                        WHERE order_id = ?
                    `, [Amount, order_id]);

                    //매도 주문 처리된 것은 주식 거래 내역에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [user_id, stock_id, price, Amount, new Date()]);

                    //처리된 매수 주문 주식 거래 내역 relation으로 이동
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, -price, Amount, new Date()]);
                    Amount = 0;
                }
                //매도주문과 매수주문의 수량이 일치하는 경우
                else{
                    //매도 주문에서 해당 주문 지우기
                    await db.promise().query(`
                        DELETE FROM sell
                        WHERE order_id = ?
                    `, [order_id]);

                    //매도 주문을 주식 거래 내역 relation에 추가
                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [user_id, stock_id, price, Amount, new Date()]);

                    await db.promise().query(`
                        INSERT INTO stock_trading_log(user_id, stock_id, buy_sell, transaction_volume, transaction_time)
                        VALUES (?,?,?,?,?)
                    `, [req.session.userID, req.session.stockID, -price, Amount, new Date()]);
                    Amount = 0;
                }
            }
        }
    }
    catch (error){
        throw error;
    }
}

router.post('/confirm', async (req, res) => {
    try{
        const {Price, Amount, isMarketPrice} = req.body;
        const [results] = await db.promise().query(`
            SELECT user_deposit
            FROM user
            WHERE user_id = ?
        `, [req.session.userID]);

        if(results[0].user_deposit < (Amount * Price)){
            res.render('menuPage', {buyOrder : 'fail'});
            return;
        }
        await stockOrder(req.session.stockID, Price, Amount, isMarketPrice, req);
        res.render('menuPage', {buyOrder : 'success'});
    }catch(error){
        throw error;
    }
});

module.exports = router;