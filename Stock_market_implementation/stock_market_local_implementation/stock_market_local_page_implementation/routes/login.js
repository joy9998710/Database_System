const express = require('express');
const router = express.Router();
const db = require('../db');

router.get('/', (req, res) => {
    res.render('loginPage');
});

router.post('/', (req, res) => {
    const {userID, userPassword} = req.body;
    db.query("SELECT * FROM user WHERE user_id = ? AND user_password = ?", [userID, userPassword], (error, results, fields) => {
        if (error) throw error;
        if(results.length > 0){
            req.session.userID = userID;
            req.session.userPassword = userPassword;
            res.render('menuPage');
        }
        else{
            res.render("loginPage", {loginResult: 'fail'});
        }
    })
})

module.exports = router;