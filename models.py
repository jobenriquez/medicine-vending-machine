from flask_sqlalchemy import SQLAlchemy
from flask_login import UserMixin
from datetime import datetime

db = SQLAlchemy()

class User(db.Model, UserMixin):
    __tablename__ = 'users'
    id = db.Column(db.Integer, primary_key=True)
    user_name = db.Column(db.String(50), unique=True, nullable=False)
    user_password = db.Column(db.String(255), nullable=False)
    full_name = db.Column(db.String(100), nullable=False)
    account_type = db.Column(db.String(20), nullable=False)

class Medicine(db.Model):
    __tablename__ = 'medicines'
    id = db.Column(db.Integer, primary_key=True)
    medicine_name = db.Column(db.String(100), nullable=False)
    expiry_date = db.Column(db.Date, nullable=False)
    price = db.Column(db.Numeric(10,2), nullable=False)
    added_date = db.Column(db.Date, nullable=False)
    row = db.Column(db.String(100), nullable=False)
    column = db.Column(db.String(100), nullable=False)


class Transaction(db.Model):
    __tablename__ = 'transactions'
    id = db.Column(db.Integer, primary_key=True)
    medicine_id = db.Column(db.Integer, nullable=False)
    medicine_name = db.Column(db.String(100), nullable=False)
    quantity_dispensed = db.Column(db.Integer, nullable=False)
    total_price = db.Column(db.Numeric(10,2), nullable=False)
    date_time = db.Column(db.DateTime, default=datetime.utcnow)
