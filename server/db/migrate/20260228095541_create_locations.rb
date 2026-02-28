class CreateLocations < ActiveRecord::Migration[8.1]
  def change
    create_table :locations do |t|
      t.integer :device_id, null: false
      t.decimal :latitude, precision: 10, scale: 7
      t.decimal :longitude, precision: 10, scale: 7
      t.decimal :altitude, precision: 8, scale: 2
      t.decimal :speed, precision: 6, scale: 2
      t.decimal :accuracy, precision: 6, scale: 2
      t.integer :battery_level
      t.decimal :battery_voltage, precision: 4, scale: 2
      t.integer :satellites
      t.decimal :hdop, precision: 5, scale: 2
      t.string :firmware_version
      t.datetime :recorded_at

      t.timestamps
    end
    add_index :locations, :device_id
  end
end
