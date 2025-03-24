const express = require('express');
const router = express.Router();
const db = require('../db');

var query = `
SELECT 
    buyer.user_id AS buyer_id,
    seller.user_id AS seller_id,
    ABS(seller.buy_sell) AS price,
    buyer.transaction_volume AS transaction_volume,
    buyer.transaction_time AS transaction_time
FROM 
    stock_trading_log AS buyer
JOIN 
    stock_trading_log AS seller ON 
    buyer.stock_id = seller.stock_id AND
    buyer.transaction_volume = seller.transaction_volume AND
    buyer.transaction_time = seller.transaction_time AND
    buyer.buy_sell < 0 AND
    seller.buy_sell > 0
JOIN 
    stock AS S ON buyer.stock_id = S.stock_id
WHERE 
    S.stock_id = ?
ORDER BY 
    buyer.transaction_time
`
    
router.post('/', (req, res) => {
    const {stockID} = req.body;
    db.query(query, [stockID], (error, results, fields) => {
        if(error) throw error;
        res.render('stockTradingLogPage', {stocks : results});
    });
});

module.exports = router;