require "test_helper"

class UserTest < ActiveSupport::TestCase
  def setup
    @user = User.new(email: "test@example.com", name: "Test User",
                     password: "password123", password_confirmation: "password123")
  end

  test "valid user" do
    assert @user.valid?
  end

  test "email required" do
    @user.email = ""
    assert_not @user.valid?
  end

  test "name required" do
    @user.name = ""
    assert_not @user.valid?
  end

  test "email uniqueness" do
    @user.save!
    duplicate = User.new(email: "test@example.com", name: "Other", password: "password123", password_confirmation: "password123")
    assert_not duplicate.valid?
  end

  test "default role is regular" do
    @user.save!
    assert @user.regular?
  end

  test "can set role to admin" do
    @user.role = :admin
    @user.save!
    assert @user.admin?
  end

  test "authenticates with correct password" do
    @user.save!
    assert @user.authenticate("password123")
  end
end
