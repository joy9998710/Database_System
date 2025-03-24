const express = require('express');
const router = express.Router();
const db = require('../db');

var query = "SELECT stock_id, stock_name, current_price, closing_price, trading_volume, ((current_price - closing_price) / closing_price) * 100 AS price_change_percentage FROM stock"

router.get('/', (req, res) => {
    db.query(query, (error, results, fields) => {
        if(error) throw error;
        res.render('stockInfoPage', {stocks : results});
    });
});

router.post('/', (req, res) => {
    const {stockName} = req.body;
    db.query("SELECT stock_id, stock_name, current_price, closing_price, trading_volume, ((current_price - closing_price) / closing_price) * 100 AS price_change_percentage FROM stock WHERE stock_name LIKE ?", ['%' + stockName + '%'], (error, results, fields) => {
        if(error) throw error;
        if(results.length > 0){
            return res.render('stockInfoPage', {stocks : results});
        }
        else{
            db.query(query, (error, total_results, fields) => {
                if(error) throw error;
                res.render('stockInfoPage', {searchValue : 'fail', stocks : total_results});
            })
        }
    })
})

module.exports = router;