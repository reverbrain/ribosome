#ifndef __IOREMAP_RIBOSOME_DISTRIBUTION_HPP
#define __IOREMAP_RIBOSOME_DISTRIBUTION_HPP

#include <Eigen/Dense>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <math.h>

namespace ioremap { namespace ribosome { namespace distribution {

struct gaussian {
	gaussian(const Eigen::VectorXd &m, const Eigen::MatrixXd &s) : mu(m), sigma(s) {
		sigma_inv = sigma.inverse();
		sigma_det = sigma.determinant();
	}

	gaussian(const std::vector<Eigen::VectorXd> &v) :
		mu(Eigen::VectorXd::Zero(v.size() != 0 ? v[0].rows() : 0)),
		sigma(Eigen::MatrixXd::Zero(mu.rows(), mu.rows()))
	{
		if (v.size() == 0)
			throw std::runtime_error("Can not create gaussian for empty input vector");

		for (auto it = v.begin(); it != v.end(); ++it) {
			mu += *it;
		}

		mu /= (double)v.size();

		for (auto it = v.begin(); it != v.end(); ++it) {
			Eigen::MatrixXd tmp = *it - mu;

			sigma += tmp * tmp.transpose();
		}

		sigma /= (double)v.size();
		sigma_inv = sigma.inverse();
		sigma_det = sigma.determinant();
	}

	double f(const Eigen::VectorXd &x) const {
		if (x.rows() != mu.rows()) {
			std::ostringstream ss;
			ss << "Can not calculate normal distribution, invalid vector: size: " << x.rows() << ", must be " << mu.size();
			throw std::runtime_error(ss.str());
		}

		return 1.0 / sqrt(pow(2 * M_PI, x.rows()) * sigma_det) * exp(-0.5 * (x - mu).transpose() * sigma_inv * (x - mu));
	}

	Eigen::Vector2d fd1(const Eigen::VectorXd &x) const {
		Eigen::Vector2d res;

		double tmp_f = f(x);
		res(0) = -2 * x(0) * tmp_f / (sigma_det); // dG/dx
		res(1) = -2 * x(1) * tmp_f / (sigma_det); // dG/dy

		return res;
	}
	
	Eigen::Vector4d fd2(const Eigen::VectorXd &x) const {
		Eigen::Vector4d res;

		double tmp_f = f(x);
		Eigen::Vector2d tmp_fd1 = fd1(x);

		res(0) = -2 / sigma_det * (tmp_f + x(0) * tmp_fd1(0)); // d2G/dx2
		res(1) = -2 / sigma_det * (tmp_f + x(1) * tmp_fd1(1)); // d2G/dy2
		res(2) = -2 / sigma_det * (x(0) * tmp_fd1(1));         // d2G/dxdy
		res(3) = -2 / sigma_det * (x(1) * tmp_fd1(0));         // d2G/dydx

		return res;
	}
	
	Eigen::VectorXd fd3(const Eigen::VectorXd &x) const {
		Eigen::VectorXd res(4);

		Eigen::Vector2d tmp_fd1 = fd1(x);
		Eigen::Vector4d tmp_fd2 = fd2(x);

		res(0) = -2 / sigma_det * (2 * tmp_fd1(0) + x(0) * tmp_fd2(0)); // d3G/dx3
		res(1) = -2 / sigma_det * (2 * tmp_fd1(1) + x(1) * tmp_fd2(1)); // d3G/dy3
		res(2) = -2 / sigma_det * (tmp_fd1(1) + x(0) * tmp_fd2(2));     // d3G/dx2dy
		res(3) = -2 / sigma_det * (tmp_fd1(0) + x(1) * tmp_fd2(3));     // d3G/dy2dx

		return res;
	}

	Eigen::VectorXd mu;
	Eigen::MatrixXd sigma, sigma_inv;
	double sigma_det;
};

} // namespace distribution

namespace filter {

template <class T>
class gaussian {
public:
	gaussian(int m, double sigma) :
		m_filter(Eigen::VectorXd::Zero(2), Eigen::MatrixXd::Identity(2, 2) * sigma),
		m_m(m),
		m_apply(2*m-1, 2*m-1)
	{
		for (int i = 0; i < m; ++i) {
			for (int j = 0; j < m; ++j) {
				Eigen::Vector2d tmp(i, j);
				double f = m_filter.f(tmp);
				m_apply(m-1 + i, m-1 + j) = f;
				m_apply(m-1 - i, m-1 + j) = f;
				m_apply(m-1 + i, m-1 - j) = f;
				m_apply(m-1 - i, m-1 - j) = f;
			}
		}

		std::cout << "filter:\n" << m_apply << std::endl;
	}

	Eigen::MatrixXd apply(const Eigen::MatrixXd &data) {
		Eigen::MatrixXd res(data.rows(), data.cols());

		for (int i = 0; i < data.rows(); ++i) {
			for (int j = 0; j < data.cols(); ++j) {
				res(i, j) = block(data, i, j).cwiseProduct(m_apply).sum();
			}
		}

		return res;
	}

private:
	T m_filter;
	int m_m;
	Eigen::MatrixXd m_apply;

	Eigen::MatrixXd block(const Eigen::MatrixXd &data, int i, int j) const {
		if ((j >= m_m - 1 && j <= data.cols() - m_m) &&
		    (i >= m_m - 1 && i <= data.rows() - m_m)) {
			return data.block(i - (m_m - 1), j - (m_m - 1), m_apply.rows(), m_apply.cols());
		}

		Eigen::MatrixXd res = Eigen::MatrixXd::Zero(m_apply.rows(), m_apply.cols());

		for (int i_idx = -m_m + 1; i_idx < m_m; ++i_idx) {
			int i_data_idx = i + i_idx;

			for (int j_idx = -m_m + 1; j_idx < m_m; ++j_idx) {
				int j_data_idx = j + j_idx;

				if ((i_data_idx >= 0 && i_data_idx < data.rows()) &&
				    (j_data_idx >= 0 && j_data_idx < data.cols())) {
					res(i_idx + m_m - 1, j_idx + m_m - 1) = data(i_data_idx, j_data_idx);
				}
			}
		}

		return res;
	}

};

}

}} // namespace ioremap::ribosome

#endif // __IOREMAP_RIBOSOME_DISTRIBUTION_HPP


