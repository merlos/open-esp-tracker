class User < ApplicationRecord
  has_secure_password
  has_many :devices, foreign_key: :user_id

  enum :role, { regular: 0, admin: 1 }, default: :regular

  validates :email, presence: true, uniqueness: { case_sensitive: false },
                    format: { with: URI::MailTo::EMAIL_REGEXP }
  validates :name, presence: true

  before_save { self.email = email.downcase }
end
