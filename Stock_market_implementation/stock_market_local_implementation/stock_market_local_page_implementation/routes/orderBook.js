const express = require('express');
const router = express.Router();
const db = require('../db');

router.post('/', async (req, res) => {
    try{
        const {stockID} = req.body;
        const [stockInfo] = await db.promise().query(`
            SELECT stock_name, current_price, price_unit
            FROM stock
            WHERE stock_id = ?
            `, [stockID]);


        const stock_name = stockInfo[0].stock_name;
        const current_price = stockInfo[0].current_price;
        const price_unit = stockInfo[0].price_unit;

        const [sellOrder] = await db.promise().query(`
            SELECT order_price, sum(order_volume) as sell_volume
            FROM sell
            WHERE stock_id = ?
            GROUP BY order_price
            ORDER BY order_price DESC
            `, [stockID]);
        
        const [buyOrder] = await db.promise().query(`
            SELECT order_price, sum(order_volume) as buy_volume
            FROM buy
            WHERE stock_id = ?
            GROUP BY order_price
            ORDER BY order_price DESC
            `, [stockID]);

        //호가창에 넣을 것들만 모은것
        const sellOrders = [];
        const buyOrders = [];

        for(var i = 0; i < 10; i++){
            const Price = current_price + (5-i) * price_unit;

            const sell_order = sellOrder.find(order => order.order_price === Price);
            sellOrders.push({price : Price, volume : (sell_order ? sell_order.sell_volume : '-')});

            const buy_order = buyOrder.find(order => order.order_price === Price);
            buyOrders.push({price : Price, volume : (buy_order ? buy_order.buy_volume : '-')});
        }
        res.render('orderBookPage', {stock_name, current_price, price_unit, sellOrders, buyOrders});
    }
    catch(error){
        throw error;
    }
});

module.exports = router;