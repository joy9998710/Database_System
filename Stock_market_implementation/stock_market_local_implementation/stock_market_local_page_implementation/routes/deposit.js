const express = require('express');
const router = express.Router();
const db = require('../db');
const MAX_DEPOSIT = 2147483647;

router.get('/', (req, res) => {
    res.render('depositPage');
});

router.post('/', (req, res) => {
    const {depositAmount} = req.body;
    const userID = req.session.userID;
    const deposit = parseInt(depositAmount);
    console.log(depositAmount);
    if(deposit< 0){
        return res.render('depositPage', {negativeValue : "negative"});
    }
    db.query("SELECT * FROM user WHERE user_id = ?", [userID], (error, results, fields) => {
        if(error) throw error;
        if(results.length > 0){
            const currentDeposit = parseInt(results[0].user_deposit);
            const newDeposit = currentDeposit + deposit;
            if(newDeposit > MAX_DEPOSIT){
                return res.render('depositPage', {negativeValue : 'overflow'});
            }
            db.query("UPDATE user SET user_deposit = ? WHERE user_id = ?", [newDeposit, userID], (error, results, fields) => {
                if(error) throw error;
                res.render('menuPage', {Deposit : 'success'});
            });
        }
    });
});

module.exports = router;