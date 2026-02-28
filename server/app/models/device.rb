class Device < ApplicationRecord
  belongs_to :user, optional: true
  has_many :locations, dependent: :destroy

  validates :name, presence: true, uniqueness: true

  def regenerate_api_token
    plain_token = SecureRandom.hex(32)
    self.api_token_prefix = plain_token[0, 8]
    self.api_token_digest = BCrypt::Password.create(plain_token)
    save!
    plain_token
  end

  def verify_api_token(plain_token)
    return false unless api_token_digest.present?
    BCrypt::Password.new(api_token_digest).is_password?(plain_token)
  end
end
