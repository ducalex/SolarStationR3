class AVGr
{
  private:
  float m_currentAvg;
  int m_count;
  
  public:
  AVGr() 
  {
    m_currentAvg = 0;
    m_count = 0;  
  }

  void add(float newValue) {
    m_count++;
    m_currentAvg = (m_currentAvg * (((float)m_count - 1) / (float)m_count)) + newValue / m_count;
  }

  float getAvg() {
    return m_currentAvg;
  }
};
