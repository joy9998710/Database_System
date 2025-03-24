const express = require('express');
const router = express.Router()
const db = require('../db');

var query = `
    SELECT S.stock_name, H.buying_price, S.current_price, H.holding_num, H.transaction_available, (((S.current_price)- (H.buying_price)) * (H.holding_num)) as Sonik, ((((S.current_price) - (H.buying_price)) / H.buying_price) * 100) as up_down
    FROM holding_stock as H JOIN stock as S ON H.stock_id = S.stock_id
    WHERE H.user_id = ?
`

var query2 = `
    SELECT S.stock_name, H.buying_price, S.current_price, H.holding_num, H.transaction_available, (((S.current_price)- (H.buying_price)) * (H.holding_num)) as Sonik, ((((S.current_price) - (H.buying_price)) / H.buying_price) * 100) as up_down
    FROM holding_stock as H JOIN stock as S ON H.stock_id = S.stock_id
    WHERE H.user_id = ? and S.stock_name LIKE ?
`
router.get('/', (req, res) => {
    db.query(query, [req.session.userID], (error, results, fields) => {
        if(error) throw error;
        if(results.length === 0){
            res.render('holdingStockPage', {stocks : []});
            return;
        }
        res.render('holdingStockPage', {stocks : results})
    })
});

router.post('/', (req, res) => {
    const {stockName} = req.body;
    db.query(query2, [req.session.userID, '%' + stockName + '%'], (error, results, fields) => {
        if(error) throw error;
        if(results.length > 0){
            res.render('holdingStockPage', {stocks : results});
        }
        else{
            db.query(query, [req.session.userID], (error, results, fields) => {
                if(error) throw error;
                return res.render('holdingStockPage', {searchValue : 'fail', stocks : results});
            });
        }
    });
});
module.exports = router;