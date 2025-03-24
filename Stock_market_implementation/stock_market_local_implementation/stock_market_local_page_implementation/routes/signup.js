const express = require('express');
const router = express.Router();
const db = require('../db');
const MAX_DEPOSIT = 2147483647;

router.get('/', (req, res) => {
    res.render('signupPage');
});

router.post('/', (req, res) => {
    const {userID, userPassword, userName, userAge, userDeposit} = req.body;
    if(userAge < 0){
        return res.render('signupPage', {signupAge : 'negative'});
    }
    else if(userDeposit < 0){
        return res.render('signupPage', {signupDeposit : 'negative'});
    }
    else if(userDeposit > MAX_DEPOSIT){
        return res.render('signupPage', {signupDeposit : 'overflow'});
    }

    db.query("SELECt * FROM user WHERE user_id = ?", [userID], (error, results, fields) => {
        if (error) throw error;
        if (results.length > 0){
            res.render('signupPage', {ExistingID : "True"});
        }
        else{
            db.query("INSERT INTO user(user_id, user_password, user_name, user_age, user_deposit) values (?, ?, ?, ?, ?)", [userID, userPassword, userName, userAge, userDeposit], (error, results, fields) => {
                if (error) throw error;
                else{
                    res.render('home', {signup : "success"});
                }
            })
        }
    })
});

module.exports = router;