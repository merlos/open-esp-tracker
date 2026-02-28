class CreateDevices < ActiveRecord::Migration[8.1]
  def change
    create_table :devices do |t|
      t.string :name, null: false
      t.string :api_token_digest
      t.string :api_token_prefix
      t.integer :user_id
      t.boolean :active, default: true

      t.timestamps
    end
    add_index :devices, :name, unique: true
    add_index :devices, :api_token_prefix
  end
end
