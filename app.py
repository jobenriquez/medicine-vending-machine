import gevent.monkey

gevent.monkey.patch_all()

import os
from dotenv import load_dotenv

from threading import Thread

from flask import Flask, render_template, request, redirect, url_for, jsonify, flash
from flask_login import (
    LoginManager,
    login_user,
    logout_user,
    login_required,
    UserMixin,
    current_user,
)

import time

from flask_bcrypt import Bcrypt

from gevent import pywsgi
from geventwebsocket.handler import WebSocketHandler

import requests
from flask_socketio import SocketIO, send

from models import db, Medicine, User, Transaction

from functools import wraps

from datetime import date, timedelta

today = date.today()

load_dotenv()
app = Flask(__name__)
app.config["SQLALCHEMY_DATABASE_URI"] = os.getenv("DATABASE_URI")
app.config["SECRET_KEY"] = os.getenv("SECRET_KEY")

socketio = SocketIO(app, async_mode="gevent", cors_allowed_origins="*")
# socketio = SocketIO(app)

db.init_app(app)
login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = "login"

bcrypt = Bcrypt(app)

@login_manager.user_loader
def load_user(user_id):
    return db.session.get(User, int(user_id))

def admin_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if not current_user.is_authenticated or current_user.account_type != "admin":
            flash("Access denied: Admins only!", "danger")
            return redirect(url_for("home"))  # Redirect non-admins to home
        return f(*args, **kwargs)

    return decorated_function


@app.route("/")
def home():
    return render_template("dashboard.html")


@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        username = request.form["username"]
        password = request.form["password"]

        user = User.query.filter_by(user_name=username).first()

        if user and bcrypt.check_password_hash(user.user_password, password):
            login_user(user)
            flash("Login successful!", "success")
            return redirect(url_for("dashboard"))
        else:
            flash("Invalid username or password", "danger")

    return render_template("login.html")


#
# @app.route('/register', methods=['GET', 'POST'])
# @login_required
# @admin_required
# def register():
#     if request.method == 'POST':
#         username = request.form['username']
#         password = request.form['password']
#         confirm_password = request.form['confirm_password']
#         full_name = request.form['full_name']
#         account_type = request.form['account_type']
#
#         # Check if passwords match
#         if password != confirm_password:
#             flash('Passwords do not match', 'danger')
#             return redirect(url_for('register'))
#
#         # Check if username already exists
#         existing_user = User.query.filter_by(user_name=username).first()
#         if existing_user:
#             flash('Username already taken. Choose a different one.', 'danger')
#             return redirect(url_for('register'))
#
#         # Hash password before storing it
#         hashed_password = bcrypt.generate_password_hash(password).decode('utf-8')
#
#         # Create a new user
#         new_user = User(user_name=username, user_password=hashed_password, full_name=full_name, account_type=account_type)
#         db.session.add(new_user)
#         db.session.commit()
#
#         flash('Registration successful! You can now log in.', 'success')
#         return redirect(url_for('login'))
#
#     return render_template('register.html')


@app.route("/logout")
@login_required
def logout():
    logout_user()
    flash("Logged out successfully", "info")
    return redirect(url_for("login"))


@app.route("/dashboard")
@login_required
@admin_required
def dashboard():
    medicines = Medicine.query.all()
    today = date.today()
    expiry_warning = today + timedelta(days=30)
    return render_template(
        "dashboard.html",
        medicines=medicines,
        today=today,
        expiry_warning=expiry_warning,
    )

@app.route("/transactions")
@login_required
@admin_required
def transactions():
    transactions = Transaction.query.all()
    return render_template(
        "transactions.html",
        transactions=transactions,
    )

@app.route("/add_item", methods=["POST"])
@login_required
@admin_required
def add_item():
    # Get form data
    medicine_name = request.form["medicine_name"]
    expiry_date = request.form["expiry_date"]
    price = request.form["price"]
    row = request.form["row"]
    column = request.form["column"]
    quantity = int(request.form["quantity"])

    # Count total number of existing items and items to be added. Total number must not exceed 4
    count = Medicine.query.filter(Medicine.row == row, Medicine.column == column).count()
    if count + quantity > 4:
        flash(
            f"Cannot add {quantity} item(s). Row {row} Column {column} can only hold {4 - count} more item(s).",
            "danger",
        )
        return redirect(url_for("dashboard"))

    for _ in range(quantity):
        new_medicine = Medicine(
            medicine_name=medicine_name,
            expiry_date=expiry_date,
            price=price,
            added_date=today,
            column=column,
            row=row,
        )
        db.session.add(new_medicine)

    try:
        db.session.commit()
        flash(f"{quantity} item(s) added successfully!", "success")
    except Exception as e:
        db.session.rollback()
        flash("Error adding items: " + str(e), "danger")

    return redirect(url_for("dashboard"))


@app.route("/delete_item/<int:item_id>", methods=["POST"])
@login_required
@admin_required
def delete_item(item_id):
    medicine = Medicine.query.get_or_404(item_id)

    try:
        db.session.delete(medicine)
        db.session.commit()
        flash("Item deleted successfully!", "success")
    except Exception as e:
        db.session.rollback()
        flash("Error deleting item: " + str(e), "danger")

    return redirect(url_for("dashboard"))

@app.route("/account_settings")
@login_required
@admin_required
def account_settings():
    return render_template("account_settings.html")


@app.route("/update_password", methods=["POST"])
@login_required
@admin_required
def update_password():
    current_password = request.form["current_password"]
    new_password = request.form["new_password"]
    confirm_new_password = request.form["confirm_new_password"]

    # Verify current password
    if not bcrypt.check_password_hash(current_user.user_password, current_password):
        flash("Invalid current password", "danger")
        return redirect(url_for("account_settings"))

    if confirm_new_password != new_password:
        flash("New password confirmation does not match", "danger")
        return redirect(url_for("account_settings"))

    # Hash the new password and update it
    hashed_password = bcrypt.generate_password_hash(new_password).decode("utf-8")
    current_user.user_password = hashed_password
    db.session.commit()

    flash("Password updated successfully!", "success")
    return redirect(url_for("account_settings"))


@socketio.on("toggle_motor")
def handle_motor_command(command):
    print(f"Received command: {command}")
    socketio.emit("toggle_motor", command)


@app.route("/dispense_item", methods=["POST"])
@login_required
@admin_required
def dispense_item():
    row = request.form["dispenseItemRow"]
    column = request.form["dispenseItemColumn"]
    quantity = int(request.form["dispenseItemQuantity"])

    # Count total number of existing items
    # count = Medicine.query.filter(Medicine.row == row, Medicine.column == column).count()
    # if quantity > count:
    #     flash(
    #         f"Cannot dispense {quantity} item(s). Row {row} Column {column} only has {count} item(s).",
    #         "danger",
    #     )
    #     return redirect(url_for("dashboard"))

    motor_position = f"{row}{column}_{quantity}"
    print(motor_position)

    socketio.emit("toggle_motor", motor_position)
    return redirect(url_for("dashboard"))

@app.route('/dispense_status', methods=['POST'])
def dispense_status():
    status = request.form.get('status')
    row = request.form.get('row')
    column = request.form.get('column')
    quantity = request.form.get('quantity')

    if status and row and column and quantity:
        print(f"Received dispense status from ESP32: {status}")
        # Only proceed if dispense was successful
        if status == "success":
            qty = int(quantity)
            # Query for the medicines in the specified location (FIFO order)
            medicines_query = Medicine.query.filter_by(row=row, column=column).order_by(Medicine.added_date.asc())
            medicines_to_dispense = medicines_query.limit(qty).all()

            if medicines_to_dispense and len(medicines_to_dispense) == qty:
                med_reference = medicines_to_dispense[0]
                total_price = float(med_reference.price) * qty

                print(f"Medicines to dispense: {medicines_to_dispense}")

                transaction = Transaction(
                    medicine_id=med_reference.id,
                    medicine_name=med_reference.medicine_name,
                    quantity_dispensed=qty,
                    total_price=total_price,
                )
                db.session.add(transaction)
                print("Medicine ID:", transaction.medicine_id)
                print("Medicine Name:", transaction.medicine_name)
                print("Quantity Dispensed:", transaction.quantity_dispensed)
                print("Total Price:", transaction.total_price)
                print("Date & Time:", transaction.date_time)

                # Remove the medicines from the medicines table
                for med in medicines_to_dispense:
                    db.session.delete(med)
                try:
                    db.session.commit()
                    flash("Medicine dispensed and transaction recorded successfully!", "success")
                except Exception as e:
                    db.session.rollback()
                    flash("Error while recording the transaction: " + str(e), "danger")

            else:
                flash("Not enough medicine items available to dispense.", "danger")
        # Emit the dispense status to connected clients (e.g., update your dashboard)
        socketio.emit('dispense_update', {'status': status})
        return 'Dispense status received', 200
    else:
        return 'Incomplete status data received', 400


if __name__ == "__main__":
    # app.run(debug=True)
    #socketio.run(app, host='0.0.0.0', port=5000, allow_unsafe_werkzeug=True)
    # socketio.run(app, host='0.0.0.0', port=5000)

    #host = "0.0.0.0"
    #host = "192.168.1.23"
    #port = 5000
    #print(f"🚀 Server running at: http://127.0.0.1:{port}/ (Localhost)")
    # print(f"🌍 Access on LAN: http://your-local-ip:{port}/")

    #server = pywsgi.WSGIServer((host, port), app, handler_class=WebSocketHandler)
    #server.serve_forever()

    host = "0.0.0.0"
    port = 5000
    print(f"🚀 Server running at: http://{host}:{port}/")
    socketio.run(app, host=host, port=port, debug=True)
