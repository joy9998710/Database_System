const express =  require('express');
const router = express.Router();
const db = require('../db');

var query = `
    SELECT S.stock_name, L.buy_sell, L.transaction_volume, L.transaction_time
    FROM stock_trading_log AS L JOIN stock AS S ON L.stock_id = S.stock_id
    WHERE L.user_id = ?
`;

var query2 = `
    SELECT S.stock_name, L.buy_sell, L.transaction_volume, L.transaction_time
    FROM stock_trading_log AS L JOIN stock AS S ON L.stock_id = S.stock_id
    WHERE L.user_id = ? and S.stock_name LIKE ?
`;

router.get('/', (req, res) => {
    db.query(query, [req.session.userID], (error, results, fields) => {
        if(error) throw error;
        if(results.length === 0){
            res.render('userTradingLogPage', {stocks : []});
            return;
        }
        res.render('userTradingLogPage', {stocks : results});
    });
});

router.post('/', (req, res) => {
    const {stockName} = req.body;
    db.query(query2, [req.session.userID, '%' + stockName + '%'], (error, results, fields) => {
        if(error) throw error;
        if(results.length > 0){
            return res.render('userTradingLogPage', {stocks : results});
        }
        else{
            db.query(query, [req.session.userID], (error, results, fields) => {
                if(error) throw error;
                res.render('userTradingLogPage', {searchValue : 'fail', stocks : results});
            });
        }
    });
});

module.exports = router;