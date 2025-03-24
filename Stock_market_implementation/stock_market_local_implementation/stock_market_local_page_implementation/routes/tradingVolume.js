const express = require('express');
const router = express.Router();
const db = require('../db');


var query = `
    SELECT stock_id, stock_name, current_price, trading_volume
    FROM stock
    ORDER BY trading_volume DESC
`
router.get('/', (req, res) => {
    db.query(query, (error, results, fields) => {
        if(error) throw error;
        res.render('tradingVolumePage', {stocks : results});
        return;
    });
});

module.exports = router;

