from flask import Flask, render_template, request
import redis


INDEX = "index.html"


app = Flask(__name__)

tags_db = redis.Redis(
    host="database", port=6379, db=0, charset="utf-8", decode_responses=True
)

attendance_db = redis.Redis(
    host="database", db=1, port=6379, charset="utf-8", decode_responses=True
)


def reload_attendance():
    keys = attendance_db.keys()
    attendance = []
    for key in keys:
        value = attendance_db.get(key)
        attendance.append({"key": key, "value": value.split("\n")})
    return attendance


def reload_data():
    keys = tags_db.keys()
    data = []
    for key in keys:
        value = tags_db.get(key)
        data.append({"key": key, "value": value})
    return data


@app.route("/", methods=["GET", "POST"])
def index():
    if request.method == "POST":
        if "add" in request.form:
            key = request.form["key"]
            value = request.form["value"]
            tags_db.set(key, value)
        elif "delete" in request.form:
            key = request.form["key"]
            tags_db.delete(key)
    return render_template(INDEX, data=reload_data(), attendance=reload_attendance())


if __name__ == "__main__":
    app.run(host="interface", port=5000, debug=True)
