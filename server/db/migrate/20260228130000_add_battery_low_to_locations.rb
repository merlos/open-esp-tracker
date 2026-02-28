class AddBatteryLowToLocations < ActiveRecord::Migration[8.1]
  def change
    add_column :locations, :battery_low, :boolean
  end
end
