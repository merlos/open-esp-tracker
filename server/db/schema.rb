# This file is auto-generated from the current state of the database. Instead
# of editing this file, please use the migrations feature of Active Record to
# incrementally modify your database, and then regenerate this schema definition.
#
# This file is the source Rails uses to define your schema when running `bin/rails
# db:schema:load`. When creating a new database, `bin/rails db:schema:load` tends to
# be faster and is potentially less error prone than running all of your
# migrations from scratch. Old migrations may fail to apply correctly if those
# migrations use external dependencies or application code.
#
# It's strongly recommended that you check this file into your version control system.

ActiveRecord::Schema[8.1].define(version: 2026_02_28_095541) do
  create_table "devices", force: :cascade do |t|
    t.boolean "active", default: true
    t.string "api_token_digest"
    t.string "api_token_prefix"
    t.datetime "created_at", null: false
    t.string "name", null: false
    t.datetime "updated_at", null: false
    t.integer "user_id"
    t.index ["api_token_prefix"], name: "index_devices_on_api_token_prefix"
    t.index ["name"], name: "index_devices_on_name", unique: true
  end

  create_table "locations", force: :cascade do |t|
    t.decimal "accuracy", precision: 6, scale: 2
    t.decimal "altitude", precision: 8, scale: 2
    t.integer "battery_level"
    t.decimal "battery_voltage", precision: 4, scale: 2
    t.datetime "created_at", null: false
    t.integer "device_id", null: false
    t.string "firmware_version"
    t.decimal "hdop", precision: 5, scale: 2
    t.decimal "latitude", precision: 10, scale: 7
    t.decimal "longitude", precision: 10, scale: 7
    t.datetime "recorded_at"
    t.integer "satellites"
    t.decimal "speed", precision: 6, scale: 2
    t.datetime "updated_at", null: false
    t.index ["device_id"], name: "index_locations_on_device_id"
  end

  create_table "users", force: :cascade do |t|
    t.datetime "created_at", null: false
    t.string "email", null: false
    t.string "name"
    t.string "password_digest", null: false
    t.integer "role", default: 0
    t.datetime "updated_at", null: false
    t.index ["email"], name: "index_users_on_email", unique: true
  end
end
