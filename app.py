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

from models import db, Medicine, User

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
    return render_template("index.html")


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

    # Count total number of existing items and items to be added. Total number must not exceed 8
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

    motor_position = f"{row}{column}_{quantity}"
    print(motor_position)

    socketio.emit("toggle_motor", motor_position)
    return redirect(url_for("dashboard"))
    # # Wait for acknowledgment with timeout
    # global acknowledged
    # acknowledged = False
    #
    # # Use threading to allow waiting asynchronously while still processing other events
    # thread = Thread(target=wait_for_acknowledgment)
    # thread.start()
    #
    # thread.join()  # Wait for the acknowledgment or timeout
    #
    # if acknowledged:
    #     return jsonify({'status': 'success', 'message': 'Item dispensed successfully.'})
    # else:
    #     return jsonify({'status': 'error', 'message': 'Failed to receive acknowledgment in time.'})


# WebSocket event handler for acknowledgment from ESP32
# @socketio.on('acknowledge')
# def handle_acknowledge(message):
#     global acknowledged
#     if message == "Process Complete":
#         print("✅ Received acknowledgment from ESP32.")
#         acknowledged = True
#
#
# def wait_for_acknowledgment():
#     start_time = time.time()
#     timeout = 60
#
#     # Block while waiting for the acknowledgment message
#     while time.time() - start_time < timeout:
#         # If acknowledgment is received, break out of the loop
#         if 'acknowledged' in globals() and acknowledged:
#             return True
#         socketio.sleep(1)  # Allow socketio to process events and avoid blocking
#
#     return False
# @socketio.on('connect')
# def handle_connect():
#    print("✅ ESP32 Connected!")
#    socketio.emit('connection_success', {'message': 'Connected to Flask WebSocket'})
#    socketio.emit('ping_test', {'message': 'Ping from Flask'})
#

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
