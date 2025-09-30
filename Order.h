#pragma once

#include <string>

class Order
{
public:
    // do not alter signature of this constructor
    Order(const std::string& ordId, const std::string& secId, const std::string& side, const unsigned int qty, const std::string& user,
          const std::string& company)
        : m_orderId(ordId), m_securityId(secId), m_side(side), m_qty(qty), m_user(user), m_company(company) { }

    // do not alter these accessor methods 
    std::string orderId() const    { return m_orderId; }
    std::string securityId() const { return m_securityId; }
    std::string side() const       { return m_side; }
    std::string user() const       { return m_user; }
    std::string company() const    { return m_company; }
    unsigned int qty() const       { return m_qty; }

private:
    // use the below to hold the order data
    // do not remove the these member variables  
    std::string m_orderId;     // unique order id
    std::string m_securityId;  // security identifier
    std::string m_side;        // side of the order, eg Buy or Sell
    unsigned int m_qty;        // qty for this order
    std::string m_user;        // user name who owns this order
    std::string m_company;     // company for user
};
